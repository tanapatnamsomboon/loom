#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/buffer.h"
#include "loom/core/log.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstring>
#include <fstream>
#include <vector>

namespace Loom {

    static const char* CgltfResultToString(cgltf_result r) {
        switch (r) {
            case cgltf_result_success:         return "success";
            case cgltf_result_data_too_short:  return "data_too_short";
            case cgltf_result_unknown_format:  return "unknown_format";
            case cgltf_result_invalid_json:    return "invalid_json";
            case cgltf_result_invalid_gltf:    return "invalid_gltf";
            case cgltf_result_invalid_options: return "invalid_options";
            case cgltf_result_file_not_found:  return "file_not_found";
            case cgltf_result_io_error:        return "io_error";
            case cgltf_result_out_of_memory:   return "out_of_memory";
            case cgltf_result_legacy_gltf:     return "legacy_gltf";
            default:                           return "unknown";
        }
    }

    std::shared_ptr<MeshAsset> MeshAsset::Create(const std::string& path) {
        cgltf_options options{};
        cgltf_data*   data = nullptr;

        cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
        if (result != cgltf_result_success) {
            LOOM_CORE_ERROR("MeshAsset: failed to parse '{}': {}", path, CgltfResultToString(result));
            return nullptr;
        }

        result = cgltf_load_buffers(&options, data, path.c_str());
        if (result != cgltf_result_success) {
            LOOM_CORE_ERROR("MeshAsset: failed to load buffers for '{}': {}{}",
                            path, CgltfResultToString(result),
                            result == cgltf_result_file_not_found
                                ? " - a .gltf references an external .bin (and possibly external textures) by relative URI; ensure those files sit next to the .gltf, or use the self-contained .glb form."
                                : "");
            cgltf_free(data);
            return nullptr;
        }

        if (cgltf_validate(data) != cgltf_result_success) {
            LOOM_CORE_ERROR("MeshAsset: validation failed for '{}'", path);
            cgltf_free(data);
            return nullptr;
        }

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t>   indices;
        bool any_uvs     = false;
        bool any_normals = false;

        // First primitive material wins — all primitives concatenate into one VAO.
        const cgltf_material* material_src = nullptr;

        // Bakes one cgltf_mesh's primitives into the shared vertex/index buffers,
        // transforming by world (positions) and inverse-transpose (normals).
        auto emit_mesh = [&](const cgltf_mesh& mesh, const glm::mat4& world,
                             const glm::mat3& normal_matrix) {
            for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi) {
                const cgltf_primitive& prim = mesh.primitives[pi];
                if (prim.type != cgltf_primitive_type_triangles) {
                    LOOM_CORE_WARN("MeshAsset: skipping non-triangle primitive in '{}'", path);
                    continue;
                }
                if (!material_src && prim.material) material_src = prim.material;

                const cgltf_accessor* pos_acc = nullptr;
                const cgltf_accessor* nrm_acc = nullptr;
                const cgltf_accessor* uv_acc  = nullptr;
                const cgltf_accessor* tan_acc = nullptr;
                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                    const cgltf_attribute& attr = prim.attributes[ai];
                    switch (attr.type) {
                        case cgltf_attribute_type_position: pos_acc = attr.data; break;
                        case cgltf_attribute_type_normal:   nrm_acc = attr.data; break;
                        case cgltf_attribute_type_texcoord: if (attr.index == 0) uv_acc = attr.data; break;
                        case cgltf_attribute_type_tangent:  tan_acc = attr.data; break;
                        default: break;
                    }
                }

                if (!pos_acc) {
                    LOOM_CORE_WARN("MeshAsset: primitive in '{}' has no POSITION attribute, skipping", path);
                    continue;
                }

                const cgltf_size  vstart = vertices.size();
                const cgltf_size  vcount = pos_acc->count;
                vertices.resize(vstart + vcount);

                if (nrm_acc) any_normals = true;
                if (uv_acc)  any_uvs     = true;

                const glm::mat3 model3 = glm::mat3(world); // for tangent transform (not inverse-transpose)

                for (cgltf_size i = 0; i < vcount; ++i) {
                    MeshVertex& v = vertices[vstart + i];

                    glm::vec3 pos_local;
                    cgltf_accessor_read_float(pos_acc, i, &pos_local.x, 3);
                    v.Position = glm::vec3(world * glm::vec4(pos_local, 1.0f));

                    glm::vec3 nrm_local = glm::vec3(0.0f, 0.0f, 1.0f);
                    if (nrm_acc) cgltf_accessor_read_float(nrm_acc, i, &nrm_local.x, 3);
                    // Renormalize: non-uniform scale stretches the normal even with the inverse-transpose remap.
                    glm::vec3 nrm_world = normal_matrix * nrm_local;
                    float     len2      = glm::dot(nrm_world, nrm_world);
                    v.Normal = (len2 > 1e-8f) ? nrm_world * glm::inversesqrt(len2)
                                              : glm::vec3(0.0f, 0.0f, 1.0f);

                    if (uv_acc) {
                        cgltf_accessor_read_float(uv_acc, i, &v.TexCoord.x, 2);
                        // V-flip: textures load with stbi flip-vertically (engine convention),
                        // so glTF top-left UVs need their V flipped to land on the right texel.
                        v.TexCoord.y = 1.0f - v.TexCoord.y;
                    } else {
                        v.TexCoord = glm::vec2(0.0f);
                    }

                    if (tan_acc) {
                        glm::vec4 tan_local;
                        cgltf_accessor_read_float(tan_acc, i, &tan_local.x, 4);
                        // glTF: tangent xyz transforms by upper-3x3, not inverse-transpose (w = handedness).
                        glm::vec3 t_world = model3 * glm::vec3(tan_local);
                        float     tlen2   = glm::dot(t_world, t_world);
                        t_world = (tlen2 > 1e-8f) ? t_world * glm::inversesqrt(tlen2)
                                                   : glm::vec3(1.0f, 0.0f, 0.0f);
                        v.Tangent = glm::vec4(t_world, tan_local.w);
                    } else {
                        v.Tangent = glm::vec4(0.0f); // sentinel — triggers Lengyel pass below
                    }
                }

                const cgltf_size istart_this_prim = indices.size();
                if (prim.indices) {
                    const cgltf_size  icount = prim.indices->count;
                    const cgltf_size  istart = indices.size();
                    indices.resize(istart + icount);
                    for (cgltf_size i = 0; i < icount; ++i) {
                        indices[istart + i] = (uint32_t)(cgltf_accessor_read_index(prim.indices, i) + vstart);
                    }
                } else {
                    indices.reserve(indices.size() + vcount);
                    for (cgltf_size i = 0; i < vcount; ++i)
                        indices.push_back((uint32_t)(vstart + i));
                }

                // Lengyel per-triangle tangent accumulation (no TANGENT attribute).
                if (!tan_acc) {
                    const cgltf_size icount_this = indices.size() - istart_this_prim;
                    std::vector<glm::vec3> tan_sum(vcount, glm::vec3(0.0f));
                    std::vector<glm::vec3> btn_sum(vcount, glm::vec3(0.0f));
                    for (cgltf_size t = 0; t < icount_this; t += 3) {
                        uint32_t i0 = indices[istart_this_prim + t    ] - (uint32_t)vstart;
                        uint32_t i1 = indices[istart_this_prim + t + 1] - (uint32_t)vstart;
                        uint32_t i2 = indices[istart_this_prim + t + 2] - (uint32_t)vstart;
                        const glm::vec3& p0 = vertices[vstart + i0].Position;
                        const glm::vec3& p1 = vertices[vstart + i1].Position;
                        const glm::vec3& p2 = vertices[vstart + i2].Position;
                        const glm::vec2& u0 = vertices[vstart + i0].TexCoord;
                        const glm::vec2& u1 = vertices[vstart + i1].TexCoord;
                        const glm::vec2& u2 = vertices[vstart + i2].TexCoord;
                        glm::vec3 e1 = p1 - p0, e2 = p2 - p0;
                        glm::vec2 d1 = u1 - u0, d2 = u2 - u0;
                        float denom = d1.x * d2.y - d2.x * d1.y;
                        float r = (glm::abs(denom) > 1e-8f) ? 1.0f / denom : 0.0f;
                        glm::vec3 T = (e1 * d2.y - e2 * d1.y) * r;
                        glm::vec3 B = (e2 * d1.x - e1 * d2.x) * r;
                        tan_sum[i0] += T; tan_sum[i1] += T; tan_sum[i2] += T;
                        btn_sum[i0] += B; btn_sum[i1] += B; btn_sum[i2] += B;
                    }
                    for (cgltf_size i = 0; i < vcount; ++i) {
                        const glm::vec3& N = vertices[vstart + i].Normal;
                        glm::vec3 T = tan_sum[i] - N * glm::dot(N, tan_sum[i]); // Gram-Schmidt
                        float tlen2 = glm::dot(T, T);
                        T = (tlen2 > 1e-8f) ? T * glm::inversesqrt(tlen2) : glm::vec3(1.0f, 0.0f, 0.0f);
                        float sign = (glm::dot(glm::cross(N, T), btn_sum[i]) < 0.0f) ? -1.0f : 1.0f;
                        vertices[vstart + i].Tangent = glm::vec4(T, sign);
                    }
                }
            }
        };

        // Bake each node's world transform into its mesh — cgltf stores vertices
        // in node-local space, so models with non-identity roots (DamagedHelmet,
        // Blender +Y-up axis-conversion) come in mis-oriented otherwise.
        auto walk_node = [&](auto& self, const cgltf_node* node) -> void {
            cgltf_float wm_raw[16];
            cgltf_node_transform_world(node, wm_raw);
            glm::mat4 world(wm_raw[0],  wm_raw[1],  wm_raw[2],  wm_raw[3],
                            wm_raw[4],  wm_raw[5],  wm_raw[6],  wm_raw[7],
                            wm_raw[8],  wm_raw[9],  wm_raw[10], wm_raw[11],
                            wm_raw[12], wm_raw[13], wm_raw[14], wm_raw[15]);
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(world)));
            if (node->mesh) emit_mesh(*node->mesh, world, nm);
            for (cgltf_size ci = 0; ci < node->children_count; ++ci)
                self(self, node->children[ci]);
        };

        bool walked_any = false;
        auto walk_scene = [&](const cgltf_scene* scene) {
            for (cgltf_size ni = 0; ni < scene->nodes_count; ++ni) {
                walk_node(walk_node, scene->nodes[ni]);
                walked_any = true;
            }
        };
        if (data->scene) {
            walk_scene(data->scene);
        } else {
            for (cgltf_size si = 0; si < data->scenes_count; ++si)
                walk_scene(&data->scenes[si]);
        }
        if (!walked_any) {
            // Defensive fallback: no scene graph (rare but legal).
            for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
                emit_mesh(data->meshes[mi], glm::mat4(1.0f), glm::mat3(1.0f));
        }

        // Material extraction must happen before cgltf_free — material_src points into `data`.
        MeshMaterial material;
        if (material_src) {
            material.HasMaterial = true;

            // External URI → fill out; embedded (data: URI or buffer view) →
            // mark the slot's *Embedded flag and leave out empty for
            // ExtractEmbeddedTextures to populate later.
            auto extract_uri = [&](const cgltf_texture* tex,
                                   std::string& out, bool& out_embedded) {
                if (!tex || !tex->image) return;
                const cgltf_image* img = tex->image;
                const char* uri = img->uri;
                if (uri && uri[0] != '\0' && std::strncmp(uri, "data:", 5) != 0) {
                    std::string decoded(uri);
                    cgltf_decode_uri(&decoded[0]);
                    decoded.resize(std::strlen(decoded.c_str()));
                    out = decoded;
                } else if ((uri && std::strncmp(uri, "data:", 5) == 0) || img->buffer_view) {
                    out_embedded = true;
                }
            };

            if (material_src->has_pbr_metallic_roughness) {
                const cgltf_pbr_metallic_roughness& pbr = material_src->pbr_metallic_roughness;
                material.BaseColorFactor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                                     pbr.base_color_factor[2], pbr.base_color_factor[3]);
                material.MetallicFactor  = pbr.metallic_factor;
                material.RoughnessFactor = pbr.roughness_factor;
                extract_uri(pbr.base_color_texture.texture,
                            material.BaseColorTexture, material.BaseColorEmbedded);
                // Engine convention: MR texture doubles as ORM (R=AO, G=rough, B=metal).
                extract_uri(pbr.metallic_roughness_texture.texture,
                            material.ORMTexture, material.ORMEmbedded);
            }

            // Separate occlusionTexture is non-ORM; warn and keep the MR import above.
            const cgltf_texture* occ_tex = material_src->occlusion_texture.texture;
            const cgltf_texture* mr_tex  = material_src->has_pbr_metallic_roughness
                ? material_src->pbr_metallic_roughness.metallic_roughness_texture.texture
                : nullptr;
            if (occ_tex && occ_tex != mr_tex) {
                LOOM_CORE_WARN("MeshAsset: '{}' provides a separate occlusionTexture "
                               "distinct from metallicRoughnessTexture. The engine uses "
                               "ORM-packed textures (R=AO, G=rough, B=metal) - re-pack "
                               "AO into the R channel of the MR texture, or re-wire the "
                               "Blender 'glTF Settings' node to point at the same image.",
                               path);
            }

            extract_uri(material_src->normal_texture.texture,
                        material.NormalTexture,   material.NormalEmbedded);
            extract_uri(material_src->emissive_texture.texture,
                        material.EmissiveTexture, material.EmissiveEmbedded);

            glm::vec3 e_factor(material_src->emissive_factor[0],
                               material_src->emissive_factor[1],
                               material_src->emissive_factor[2]);
            // Fold KHR_materials_emissive_strength into the factor at import time.
            if (material_src->has_emissive_strength)
                e_factor *= material_src->emissive_strength.emissive_strength;
            material.EmissiveFactor = e_factor;
        }

        cgltf_free(data);

        if (vertices.empty() || indices.empty()) {
            LOOM_CORE_ERROR("MeshAsset: '{}' produced no geometry", path);
            return nullptr;
        }

        auto vbo = VertexBuffer::Create(vertices.data(), (uint32_t)(vertices.size() * sizeof(MeshVertex)));
        vbo->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal"   },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float4, "a_Tangent"  },
        });

        auto ibo = IndexBuffer::Create(indices.data(), (uint32_t)indices.size());

        auto vao = VertexArray::Create();
        vao->AddVertexBuffer(vbo);
        vao->SetIndexBuffer(ibo);

        auto asset = std::make_shared<MeshAsset>();
        asset->mVertexArray = std::move(vao);
        asset->mPath        = path;
        asset->mVertexCount = (uint32_t)vertices.size();
        asset->mIndexCount  = (uint32_t)indices.size();
        asset->mMaterial    = material;

        LOOM_CORE_TRACE("MeshAsset: loaded '{}' ({} vertices, {} indices, normals={}, uvs={}, material={})",
                        path, asset->mVertexCount, asset->mIndexCount,
                        any_normals ? "yes" : "NO (defaulted to +Z)",
                        any_uvs     ? "yes" : "NO (tangents generated from geometry only; normal map will be flat)",
                        material.HasMaterial ? "yes" : "none");
        if (!any_uvs)
            LOOM_CORE_WARN("MeshAsset: '{}' has no TEXCOORD_0 attribute. Albedo texture sampling will be flat. "
                           "Re-export the model with UVs (Blender: 'UV -> Smart UV Project' or 'Cube Projection' before glTF export).", path);
        return asset;
    }

    // ---- Embedded-texture extraction --------------------------------------

    namespace {
        const char* MimeToExt(const char* mime) {
            if (!mime) return ".bin";
            if (std::strcmp(mime, "image/png")  == 0) return ".png";
            if (std::strcmp(mime, "image/jpeg") == 0) return ".jpg";
            if (std::strcmp(mime, "image/bmp")  == 0) return ".bmp";
            return ".bin";
        }

        // Sniffs a magic-number header when the MIME type is missing — common
        // for buffer-view images that omit the mimeType field.
        const char* SniffExt(const uint8_t* data, size_t size) {
            if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
                return ".png";
            if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
                return ".jpg";
            if (size >= 2 && data[0] == 'B' && data[1] == 'M')
                return ".bmp";
            return ".bin";
        }

        // RFC 4648 base64 decode; tolerates standard alphabet only (glTF spec).
        std::vector<uint8_t> Base64Decode(const char* src, size_t len) {
            static int8_t table[256];
            static bool  init = false;
            if (!init) {
                std::memset(table, -1, sizeof(table));
                const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                for (int i = 0; i < 64; ++i) table[(uint8_t)alpha[i]] = (int8_t)i;
                init = true;
            }
            std::vector<uint8_t> out;
            out.reserve(len * 3 / 4);
            uint32_t buf = 0;
            int      bits = 0;
            for (size_t i = 0; i < len; ++i) {
                uint8_t c = (uint8_t)src[i];
                if (c == '=') break;
                int8_t v = table[c];
                if (v < 0) continue; // skip whitespace / invalid
                buf = (buf << 6) | (uint32_t)v;
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    out.push_back((uint8_t)((buf >> bits) & 0xFF));
                }
            }
            return out;
        }
    } // namespace

    int MeshAsset::ExtractEmbeddedTextures(const std::string&            mesh_path,
                                           const std::filesystem::path&  dest_dir,
                                           const std::string&            filename_prefix,
                                           MeshMaterial&                 io_material) {
        cgltf_options options{};
        cgltf_data*   data = nullptr;
        if (cgltf_parse_file(&options, mesh_path.c_str(), &data) != cgltf_result_success) {
            LOOM_CORE_ERROR("MeshAsset::ExtractEmbeddedTextures: parse failed for '{}'", mesh_path);
            return 0;
        }
        if (cgltf_load_buffers(&options, data, mesh_path.c_str()) != cgltf_result_success) {
            LOOM_CORE_ERROR("MeshAsset::ExtractEmbeddedTextures: buffer load failed for '{}'", mesh_path);
            cgltf_free(data);
            return 0;
        }

        std::error_code ec;
        std::filesystem::create_directories(dest_dir, ec);

        // Mirror Create()'s "first primitive material wins" with the same
        // scene-graph traversal — otherwise multi-material assets risk picking
        // a different material than the one whose embedded flags the inspector
        // is acting on.
        const cgltf_material* mat = nullptr;
        auto find_first = [&](auto& self, const cgltf_node* node) -> void {
            if (mat) return;
            if (node->mesh) {
                for (cgltf_size pi = 0; pi < node->mesh->primitives_count && !mat; ++pi)
                    if (node->mesh->primitives[pi].material)
                        mat = node->mesh->primitives[pi].material;
            }
            for (cgltf_size ci = 0; ci < node->children_count && !mat; ++ci)
                self(self, node->children[ci]);
        };
        if (data->scene) {
            for (cgltf_size ni = 0; ni < data->scene->nodes_count && !mat; ++ni)
                find_first(find_first, data->scene->nodes[ni]);
        } else {
            for (cgltf_size si = 0; si < data->scenes_count && !mat; ++si)
                for (cgltf_size ni = 0; ni < data->scenes[si].nodes_count && !mat; ++ni)
                    find_first(find_first, data->scenes[si].nodes[ni]);
        }
        if (!mat) {
            for (cgltf_size mi = 0; mi < data->meshes_count && !mat; ++mi)
                for (cgltf_size pi = 0; pi < data->meshes[mi].primitives_count && !mat; ++pi)
                    if (data->meshes[mi].primitives[pi].material)
                        mat = data->meshes[mi].primitives[pi].material;
        }
        if (!mat) {
            cgltf_free(data);
            return 0;
        }

        int written = 0;

        auto write_slot = [&](const cgltf_texture* tex, const char* slot_name,
                              std::string& out_uri, bool& flag) -> void {
            if (!flag || !tex || !tex->image) return;
            const cgltf_image* img = tex->image;
            std::vector<uint8_t> bytes;
            std::string mime_buf;
            const char* mime = img->mime_type;

            if (img->buffer_view) {
                const cgltf_buffer_view* bv = img->buffer_view;
                if (!bv->buffer || !bv->buffer->data) {
                    LOOM_CORE_WARN("ExtractEmbeddedTextures: {} buffer-view has no data", slot_name);
                    return;
                }
                const uint8_t* src = (const uint8_t*)bv->buffer->data + bv->offset;
                bytes.assign(src, src + bv->size);
            } else if (img->uri && std::strncmp(img->uri, "data:", 5) == 0) {
                const char* comma = std::strchr(img->uri, ',');
                if (!comma) {
                    LOOM_CORE_WARN("ExtractEmbeddedTextures: {} data-URI malformed", slot_name);
                    return;
                }
                // Pull MIME from "data:<mime>;base64": between offset 5 and the first ';' (or ',').
                if (!mime) {
                    const char* mime_start = img->uri + 5;
                    const char* mime_end = std::strchr(mime_start, ';');
                    if (!mime_end || mime_end > comma) mime_end = comma;
                    mime_buf.assign(mime_start, (size_t)(mime_end - mime_start));
                    mime = mime_buf.c_str();
                }
                bytes = Base64Decode(comma + 1, std::strlen(comma + 1));
            } else {
                return;
            }

            const char* ext = mime ? MimeToExt(mime) : SniffExt(bytes.data(), bytes.size());
            std::string filename = filename_prefix + "_" + slot_name + ext;
            std::filesystem::path out_path = dest_dir / filename;

            std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                LOOM_CORE_ERROR("ExtractEmbeddedTextures: cannot open '{}' for write",
                                out_path.generic_string());
                return;
            }
            ofs.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
            ofs.close();

            out_uri = filename;
            flag    = false;
            ++written;
            LOOM_CORE_INFO("ExtractEmbeddedTextures: wrote '{}' ({} bytes)",
                           out_path.generic_string(), bytes.size());
        };

        if (mat->has_pbr_metallic_roughness) {
            write_slot(mat->pbr_metallic_roughness.base_color_texture.texture, "base_color",
                       io_material.BaseColorTexture, io_material.BaseColorEmbedded);
            write_slot(mat->pbr_metallic_roughness.metallic_roughness_texture.texture, "orm",
                       io_material.ORMTexture, io_material.ORMEmbedded);
        }
        write_slot(mat->normal_texture.texture,   "normal",
                   io_material.NormalTexture,   io_material.NormalEmbedded);
        write_slot(mat->emissive_texture.texture, "emissive",
                   io_material.EmissiveTexture, io_material.EmissiveEmbedded);

        cgltf_free(data);
        return written;
    }

    void MeshAsset::Reload() {
        auto fresh = MeshAsset::Create(mPath);
        if (!fresh) {
            LOOM_CORE_ERROR("MeshAsset: reload failed for '{}', keeping previous data", mPath);
            return;
        }
        mVertexArray = std::move(fresh->mVertexArray);
        mVertexCount = fresh->mVertexCount;
        mIndexCount  = fresh->mIndexCount;
        mMaterial    = std::move(fresh->mMaterial);
        LOOM_CORE_INFO("MeshAsset: hot-reloaded '{}'", mPath);
    }

} // namespace Loom
