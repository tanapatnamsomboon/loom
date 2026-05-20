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

        for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
            const cgltf_mesh& mesh = data->meshes[mi];
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
                    cgltf_accessor_read_float(pos_acc, i, &v.Position.x, 3);
                    if (nrm_acc) cgltf_accessor_read_float(nrm_acc, i, &v.Normal.x, 3);
                    else         v.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
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
        }

        // ── Material (pbrMetallicRoughness) ──────────────────────────────
        // Extracted before cgltf_free since material_src points into `data`.
        MeshMaterial material;
        if (material_src && material_src->has_pbr_metallic_roughness) {
            const cgltf_pbr_metallic_roughness& pbr = material_src->pbr_metallic_roughness;
            material.HasMaterial     = true;
            material.BaseColorFactor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                                 pbr.base_color_factor[2], pbr.base_color_factor[3]);
            material.MetallicFactor  = pbr.metallic_factor;
            material.RoughnessFactor = pbr.roughness_factor;

            const cgltf_texture* base_tex = pbr.base_color_texture.texture;
            const char*          uri      = (base_tex && base_tex->image) ? base_tex->image->uri : nullptr;
            if (uri && uri[0] != '\0' && std::strncmp(uri, "data:", 5) != 0) {
                // glTF URIs may be percent-encoded; decode in a mutable copy.
                std::string decoded(uri);
                cgltf_decode_uri(&decoded[0]);
                decoded.resize(std::strlen(decoded.c_str()));
                material.BaseColorTexture = decoded;
            } else if (base_tex && base_tex->image) {
                LOOM_CORE_WARN("MeshAsset: '{}' has an embedded base-color texture — "
                               "import will bring factors only; extract the texture and "
                               "assign it manually for textured albedo.", path);
            }
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

} // namespace Loom
