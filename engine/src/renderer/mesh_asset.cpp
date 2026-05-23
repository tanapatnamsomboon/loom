#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/buffer.h"
#include "loom/core/log.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstring>
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

        // First primitive material wins — our import concatenates every
        // primitive into a single VAO, so the mesh carries one material.
        const cgltf_material* material_src = nullptr;

        // Bakes one cgltf_mesh's primitives into the shared vertex / index
        // buffers, transforming positions by `world` and normals by the
        // inverse-transpose of its upper 3x3 (so non-uniform scale doesn't
        // skew them). Hoisted out of the scene-graph walk below so a node
        // and a no-scene-graph fallback can share the same code path.
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
                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                    const cgltf_attribute& attr = prim.attributes[ai];
                    switch (attr.type) {
                        case cgltf_attribute_type_position: pos_acc = attr.data; break;
                        case cgltf_attribute_type_normal:   nrm_acc = attr.data; break;
                        case cgltf_attribute_type_texcoord: if (attr.index == 0) uv_acc = attr.data; break;
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

                for (cgltf_size i = 0; i < vcount; ++i) {
                    MeshVertex& v = vertices[vstart + i];

                    glm::vec3 pos_local;
                    cgltf_accessor_read_float(pos_acc, i, &pos_local.x, 3);
                    v.Position = glm::vec3(world * glm::vec4(pos_local, 1.0f));

                    glm::vec3 nrm_local = glm::vec3(0.0f, 0.0f, 1.0f);
                    if (nrm_acc) cgltf_accessor_read_float(nrm_acc, i, &nrm_local.x, 3);
                    // Renormalize after transform — non-uniform scale stretches
                    // the normal even with the inverse-transpose remap.
                    glm::vec3 nrm_world = normal_matrix * nrm_local;
                    float     len2      = glm::dot(nrm_world, nrm_world);
                    v.Normal = (len2 > 1e-8f) ? nrm_world * glm::inversesqrt(len2)
                                              : glm::vec3(0.0f, 0.0f, 1.0f);

                    if (uv_acc) {
                        cgltf_accessor_read_float(uv_acc, i, &v.TexCoord.x, 2);
                        // glTF UV origin is top-left (+V down). The engine loads
                        // every texture with stbi flip-vertically-on-load (so
                        // Renderer2D's bottom-left quad UVs show sprites upright),
                        // which puts GL t=0 at the image bottom. Flip mesh V here
                        // so glTF UVs land on the correct texel rows.
                        v.TexCoord.y = 1.0f - v.TexCoord.y;
                    } else {
                        v.TexCoord = glm::vec2(0.0f);
                    }
                }

                if (prim.indices) {
                    const cgltf_size  icount = prim.indices->count;
                    const cgltf_size  istart = indices.size();
                    indices.resize(istart + icount);
                    for (cgltf_size i = 0; i < icount; ++i) {
                        indices[istart + i] = (uint32_t)(cgltf_accessor_read_index(prim.indices, i) + vstart);
                    }
                } else {
                    // Non-indexed primitive: emit a sequential index list.
                    indices.reserve(indices.size() + vcount);
                    for (cgltf_size i = 0; i < vcount; ++i)
                        indices.push_back((uint32_t)(vstart + i));
                }
            }
        };

        // Walk the glTF scene graph, baking each node's world transform into
        // its referenced mesh. Without this, models that ship with a non-
        // identity root node (e.g. DamagedHelmet, or anything exported from
        // Blender's "+Y up" preset which inserts a root axis-conversion
        // rotation) come in mis-oriented because cgltf stores mesh vertices
        // in node-local space.
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
            // Defensive fallback: file has no scene graph (rare, but legal).
            // Walk all meshes with identity transform — preserves the
            // pre-fix behavior so we don't regress anything that loaded before.
            for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
                emit_mesh(data->meshes[mi], glm::mat4(1.0f), glm::mat3(1.0f));
        }

        // ── Material (pbrMetallicRoughness + occlusion + emissive) ───────
        // Extracted before cgltf_free since material_src points into `data`.
        MeshMaterial material;
        if (material_src) {
            material.HasMaterial = true;

            // Resolves a glTF texture-view's external URI into `out`, or warns
            // (and leaves `out` empty) when the texture is embedded (.glb
            // buffer-view / data-URI) — those would need stb-from-memory decode,
            // tracked separately on the roadmap.
            auto extract_uri = [&](const cgltf_texture* tex, const char* label,
                                   std::string& out) {
                const char* uri = (tex && tex->image) ? tex->image->uri : nullptr;
                if (uri && uri[0] != '\0' && std::strncmp(uri, "data:", 5) != 0) {
                    std::string decoded(uri);
                    cgltf_decode_uri(&decoded[0]);
                    decoded.resize(std::strlen(decoded.c_str()));
                    out = decoded;
                } else if (tex && tex->image) {
                    LOOM_CORE_WARN("MeshAsset: '{}' has an embedded {} texture — "
                                   "import will bring factors only; extract the texture "
                                   "and assign it manually.", path, label);
                }
            };

            if (material_src->has_pbr_metallic_roughness) {
                const cgltf_pbr_metallic_roughness& pbr = material_src->pbr_metallic_roughness;
                material.BaseColorFactor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                                     pbr.base_color_factor[2], pbr.base_color_factor[3]);
                material.MetallicFactor  = pbr.metallic_factor;
                material.RoughnessFactor = pbr.roughness_factor;
                extract_uri(pbr.base_color_texture.texture,         "base-color",
                            material.BaseColorTexture);
                // The metallic-roughness texture *is* the ORM texture in our
                // model. R is also used as AO; G/B are roughness/metallic.
                extract_uri(pbr.metallic_roughness_texture.texture, "ORM",
                            material.ORMTexture);
            }

            // glTF also exposes occlusionTexture as a separate slot. The ORM
            // convention reuses the metallic-roughness texture for AO (R), so
            // when both slots reference the same image we silently treat it
            // as the ORM map. When they point to *different* images, the
            // author hasn't followed the convention — log a warning and stick
            // with the MR texture (which we already imported above).
            const cgltf_texture* occ_tex = material_src->occlusion_texture.texture;
            const cgltf_texture* mr_tex  = material_src->has_pbr_metallic_roughness
                ? material_src->pbr_metallic_roughness.metallic_roughness_texture.texture
                : nullptr;
            if (occ_tex && occ_tex != mr_tex) {
                LOOM_CORE_WARN("MeshAsset: '{}' provides a separate occlusionTexture "
                               "distinct from metallicRoughnessTexture. The engine uses "
                               "ORM-packed textures (R=AO, G=rough, B=metal) — re-pack "
                               "AO into the R channel of the MR texture, or re-wire the "
                               "Blender 'glTF Settings' node to point at the same image.",
                               path);
            }

            // Emissive lives on cgltf_material directly, not nested inside
            // pbrMetallicRoughness, so it applies to any material.
            extract_uri(material_src->emissive_texture.texture,  "emissive",
                        material.EmissiveTexture);

            glm::vec3 e_factor(material_src->emissive_factor[0],
                               material_src->emissive_factor[1],
                               material_src->emissive_factor[2]);
            // KHR_materials_emissive_strength multiplies the factor to push it
            // into HDR territory. We fold it into the factor at import time so
            // consumers can apply it verbatim.
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
                        any_uvs     ? "yes" : "NO (defaulted to (0,0) - albedo texture will show as flat color)",
                        material.HasMaterial ? "yes" : "none");
        if (!any_uvs)
            LOOM_CORE_WARN("MeshAsset: '{}' has no TEXCOORD_0 attribute. Albedo texture sampling will be flat. "
                           "Re-export the model with UVs (Blender: 'UV -> Smart UV Project' or 'Cube Projection' before glTF export).", path);
        return asset;
    }

    void MeshAsset::Reload() {
        // Re-run the importer through Create() so we share one code path for
        // parsing + scene-graph baking + material extraction. On success, swap
        // internals so existing shared_ptr holders pick up the refreshed data.
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
