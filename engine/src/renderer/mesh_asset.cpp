#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/buffer.h"
#include "loom/core/log.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
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

        // Parallel-to-`vertices` skin attributes: only populated when a primitive
        // carries JOINTS_0 / WEIGHTS_0. We commit a separate skin VBO at the end
        // iff any primitive contributed real skin data (a static mesh leaves
        // these empty and gets the standard single-VBO layout).
        //
        // Joints are stored as ivec4 (i32, not the spec's u8/u16) — the engine's
        // ShaderDataType doesn't have an unsigned-int slot, and 128-joint cap
        // fits ivec4 trivially. Weights stay as vec4.
        struct SkinVertex { glm::ivec4 Joints; glm::vec4 Weights; };
        std::vector<SkinVertex> skin_attribs;
        bool any_skin       = false;
        const cgltf_skin* skin_src = nullptr; // first primitive's skin wins, mirroring material policy

        // First primitive material wins — all primitives concatenate into one VAO.
        const cgltf_material* material_src = nullptr;

        // Bakes one cgltf_mesh's primitives into the shared vertex/index buffers,
        // transforming by world (positions) and inverse-transpose (normals).
        // Skinned primitives override world->identity locally: glTF specifies
        // that node transforms are ignored when a mesh is skinned (the skin's
        // joint transforms handle all positioning).
        auto emit_mesh = [&](const cgltf_mesh& mesh,
                             const cgltf_skin* node_skin,
                             const glm::mat4& world_in,
                             const glm::mat3& normal_matrix_in) {
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
                const cgltf_accessor* joints_acc  = nullptr;
                const cgltf_accessor* weights_acc = nullptr;
                for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                    const cgltf_attribute& attr = prim.attributes[ai];
                    switch (attr.type) {
                        case cgltf_attribute_type_position: pos_acc = attr.data; break;
                        case cgltf_attribute_type_normal:   nrm_acc = attr.data; break;
                        case cgltf_attribute_type_texcoord: if (attr.index == 0) uv_acc = attr.data; break;
                        case cgltf_attribute_type_tangent:  tan_acc = attr.data; break;
                        case cgltf_attribute_type_joints:   if (attr.index == 0) joints_acc  = attr.data; break;
                        case cgltf_attribute_type_weights:  if (attr.index == 0) weights_acc = attr.data; break;
                        default: break;
                    }
                }

                // Skinned iff the primitive carries JOINTS_0 + WEIGHTS_0 AND
                // the owning node has a skin. (A primitive with JOINTS_0 but
                // no node->skin is malformed glTF — treat as static.)
                const bool prim_skinned = (joints_acc && weights_acc && node_skin != nullptr);
                if (prim_skinned) {
                    any_skin = true;
                    if (!skin_src) skin_src = node_skin;
                }

                const glm::mat4 world         = prim_skinned ? glm::mat4(1.0f) : world_in;
                const glm::mat3 normal_matrix = prim_skinned ? glm::mat3(1.0f) : normal_matrix_in;

                if (!pos_acc) {
                    LOOM_CORE_WARN("MeshAsset: primitive in '{}' has no POSITION attribute, skipping", path);
                    continue;
                }

                const cgltf_size  vstart = vertices.size();
                const cgltf_size  vcount = pos_acc->count;
                vertices.resize(vstart + vcount);

                // Skin attribs run parallel to `vertices`. Static primitives
                // append zeroed entries so per-vertex indices line up if a
                // later primitive turns out to be skinned. The whole buffer is
                // committed iff any primitive was skinned.
                skin_attribs.resize(vstart + vcount);

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

                    SkinVertex& sv = skin_attribs[vstart + i];
                    if (prim_skinned) {
                        cgltf_uint    j[4]  = { 0, 0, 0, 0 };
                        cgltf_float   w[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
                        cgltf_accessor_read_uint (joints_acc,  i, j, 4);
                        cgltf_accessor_read_float(weights_acc, i, w, 4);
                        // Weights should sum to 1.0 per glTF spec but exporters
                        // occasionally drift; renormalize defensively.
                        float wsum = w[0] + w[1] + w[2] + w[3];
                        float inv  = (wsum > 1e-6f) ? (1.0f / wsum) : 0.0f;
                        sv.Joints  = glm::ivec4((int)j[0], (int)j[1], (int)j[2], (int)j[3]);
                        sv.Weights = glm::vec4(w[0] * inv, w[1] * inv, w[2] * inv, w[3] * inv);
                    } else {
                        sv.Joints  = glm::ivec4(0, 0, 0, 0);
                        sv.Weights = glm::vec4(0.0f);
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
            if (node->mesh) emit_mesh(*node->mesh, node->skin, world, nm);
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
                emit_mesh(data->meshes[mi], nullptr, glm::mat4(1.0f), glm::mat3(1.0f));
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

        // Build the Skeleton from the first skin we found (must happen before
        // cgltf_free — skin_src points into `data`).
        Skeleton skeleton;
        if (skin_src) {
            const cgltf_size raw_joint_count = skin_src->joints_count;
            const cgltf_size joint_count     = (raw_joint_count > (cgltf_size)Skeleton::kMaxJoints)
                                               ? (cgltf_size)Skeleton::kMaxJoints
                                               : raw_joint_count;
            if (raw_joint_count > (cgltf_size)Skeleton::kMaxJoints) {
                LOOM_CORE_WARN("MeshAsset: '{}' skin has {} joints; engine cap is {}. "
                               "Extra joints will be clamped to index 0 - animation will be wrong "
                               "for vertices weighted to clamped joints.",
                               path, raw_joint_count, Skeleton::kMaxJoints);
            }
            skeleton.Joints.resize(joint_count);

            // Map cgltf_node* -> joint index, for parent-link resolution below.
            // Joints not in this skin produce -1 (root-like behavior).
            auto node_to_joint = [&](const cgltf_node* n) -> int {
                for (cgltf_size j = 0; j < joint_count; ++j)
                    if (skin_src->joints[j] == n) return (int)j;
                return -1;
            };

            for (cgltf_size j = 0; j < joint_count; ++j) {
                const cgltf_node* jn = skin_src->joints[j];
                SkeletonJoint&    sj = skeleton.Joints[j];

                // Local bind = the joint node's local TRS. cgltf gives us a
                // local transform via cgltf_node_transform_local; we also
                // read the individual T/R/S arrays so the runtime sampler can
                // use them as defaults for animation tracks that don't animate
                // every channel.
                cgltf_float local_m[16];
                cgltf_node_transform_local(jn, local_m);
                sj.LocalBind = glm::mat4(local_m[0],  local_m[1],  local_m[2],  local_m[3],
                                         local_m[4],  local_m[5],  local_m[6],  local_m[7],
                                         local_m[8],  local_m[9],  local_m[10], local_m[11],
                                         local_m[12], local_m[13], local_m[14], local_m[15]);
                sj.BindTranslation = glm::vec3(jn->translation[0], jn->translation[1], jn->translation[2]);
                // glTF rotation array is XYZW; glm::quat is WXYZ.
                sj.BindRotation    = glm::quat(jn->rotation[3], jn->rotation[0], jn->rotation[1], jn->rotation[2]);
                sj.BindScale       = glm::vec3(jn->scale[0], jn->scale[1], jn->scale[2]);

                // Parent is the joint's scene-graph parent IF that parent is
                // also in this skin's joint set. If not, treat as root (-1).
                sj.Parent = (jn->parent ? node_to_joint(jn->parent) : -1);

                sj.Name = jn->name ? jn->name : ("joint_" + std::to_string(j));
            }

            // Inverse bind matrices: optional in glTF (default = identity per
            // joint), provided as a tightly-packed float16 accessor when present.
            if (skin_src->inverse_bind_matrices) {
                const cgltf_accessor* ibm = skin_src->inverse_bind_matrices;
                for (cgltf_size j = 0; j < joint_count && j < ibm->count; ++j) {
                    cgltf_float m[16];
                    cgltf_accessor_read_float(ibm, j, m, 16);
                    skeleton.Joints[j].InverseBind = glm::mat4(
                        m[0],  m[1],  m[2],  m[3],
                        m[4],  m[5],  m[6],  m[7],
                        m[8],  m[9],  m[10], m[11],
                        m[12], m[13], m[14], m[15]);
                }
            }
            // Capture the world transform of any non-joint ancestor chain
            // above the skin's root joint(s). glTF exporters (Blender et al.)
            // routinely tuck a Z-up to Y-up rotation onto a "Skeleton" /
            // "Armature" node that wraps the joint hierarchy. Without this,
            // root joints would render in the model's pre-rotation frame
            // (character lying horizontally instead of standing upright).
            const cgltf_node* above_root = nullptr;
            for (cgltf_size j = 0; j < joint_count; ++j) {
                if (skeleton.Joints[j].Parent < 0) {
                    above_root = skin_src->joints[j]->parent;
                    break;
                }
            }
            if (above_root) {
                cgltf_float m[16];
                cgltf_node_transform_world(above_root, m);
                skeleton.RootWorld = glm::mat4(m[0],  m[1],  m[2],  m[3],
                                               m[4],  m[5],  m[6],  m[7],
                                               m[8],  m[9],  m[10], m[11],
                                               m[12], m[13], m[14], m[15]);
            }

            LOOM_CORE_TRACE("MeshAsset: '{}' skin loaded - {} joints, IBM={}, root_above={}",
                            path, joint_count,
                            skin_src->inverse_bind_matrices ? "yes" : "no (identity defaulted)",
                            above_root ? "yes" : "no");
        }

        // ── Animation clip extraction ───────────────────────────────────────
        // Only meaningful when we have a skin (otherwise there are no joints
        // for animation channels to target). Channels not targeting joints in
        // this skin are silently dropped; weight/morph channels are skipped.
        std::vector<AnimationClip3D> clips;
        if (skin_src && skeleton.JointCount() > 0) {
            auto node_to_joint = [&](const cgltf_node* n) -> int {
                for (int j = 0; j < skeleton.JointCount(); ++j)
                    if (skin_src->joints[j] == n) return j;
                return -1;
            };

            auto cgltf_interp_to_engine = [](cgltf_interpolation_type t) {
                switch (t) {
                    case cgltf_interpolation_type_linear:       return Interpolation::Linear;
                    case cgltf_interpolation_type_step:         return Interpolation::Step;
                    case cgltf_interpolation_type_cubic_spline: return Interpolation::CubicSpline;
                    default:                                    return Interpolation::Linear;
                }
            };

            for (cgltf_size ai = 0; ai < data->animations_count; ++ai) {
                const cgltf_animation& anim = data->animations[ai];
                AnimationClip3D clip;
                clip.Name = anim.name ? anim.name : ("animation_" + std::to_string(ai));
                // Disambiguate duplicate names by suffixing the animation index.
                for (const auto& existing : clips) {
                    if (existing.Name == clip.Name) {
                        clip.Name += "_" + std::to_string(ai);
                        break;
                    }
                }

                std::vector<JointAnimationTrack> tracks_by_joint(skeleton.JointCount());
                for (int j = 0; j < skeleton.JointCount(); ++j)
                    tracks_by_joint[j].JointIndex = j;

                bool any_track = false;
                for (cgltf_size ci = 0; ci < anim.channels_count; ++ci) {
                    const cgltf_animation_channel& chan = anim.channels[ci];
                    if (!chan.target_node || !chan.sampler) continue;
                    int joint = node_to_joint(chan.target_node);
                    if (joint < 0) continue; // channel targets non-joint node
                    if (chan.target_path == cgltf_animation_path_type_weights) continue; // morphs unsupported

                    const cgltf_animation_sampler& samp = *chan.sampler;
                    if (!samp.input || !samp.output) continue;

                    JointChannel jc;
                    jc.Interp = cgltf_interp_to_engine(samp.interpolation);
                    const cgltf_size key_count = samp.input->count;
                    jc.Times.resize(key_count);
                    for (cgltf_size k = 0; k < key_count; ++k)
                        cgltf_accessor_read_float(samp.input, k, &jc.Times[k], 1);

                    // CubicSpline: output has 3*key_count entries (in_tangent,
                    // value, out_tangent). We keep only the value slice; tangents
                    // are dropped for slice 2 (renders as polyline-with-corners
                    // through the keyframes, visually fine for most rigs).
                    const cgltf_size stride = (samp.interpolation == cgltf_interpolation_type_cubic_spline) ? 3 : 1;
                    const cgltf_size value_offset = (stride == 3) ? 1 : 0;

                    // Update duration BEFORE the std::move below — read from
                    // the cgltf accessor directly so we don't touch jc.Times
                    // post-move.
                    if (samp.input->count > 0) {
                        float last_t = 0.0f;
                        cgltf_accessor_read_float(samp.input, samp.input->count - 1, &last_t, 1);
                        if (last_t > clip.Duration) clip.Duration = last_t;
                    }

                    if (chan.target_path == cgltf_animation_path_type_rotation) {
                        jc.ValuesQuat.resize(key_count);
                        for (cgltf_size k = 0; k < key_count; ++k) {
                            float xyzw[4];
                            cgltf_accessor_read_float(samp.output, k * stride + value_offset, xyzw, 4);
                            // glTF stores rotation quats as XYZW; glm::quat is WXYZ.
                            jc.ValuesQuat[k] = glm::quat(xyzw[3], xyzw[0], xyzw[1], xyzw[2]);
                        }
                        tracks_by_joint[joint].Rotation = std::move(jc);
                    } else if (chan.target_path == cgltf_animation_path_type_translation ||
                               chan.target_path == cgltf_animation_path_type_scale) {
                        jc.ValuesVec3.resize(key_count);
                        for (cgltf_size k = 0; k < key_count; ++k) {
                            cgltf_accessor_read_float(samp.output, k * stride + value_offset,
                                                      &jc.ValuesVec3[k].x, 3);
                        }
                        if (chan.target_path == cgltf_animation_path_type_translation)
                            tracks_by_joint[joint].Translation = std::move(jc);
                        else
                            tracks_by_joint[joint].Scale = std::move(jc);
                    }
                    any_track = true;
                }

                if (!any_track) continue;

                // Drop tracks whose joint has no animated channel.
                for (auto& t : tracks_by_joint) {
                    if (t.Translation.Empty() && t.Rotation.Empty() && t.Scale.Empty()) continue;
                    clip.Tracks.push_back(std::move(t));
                }

                if (!clip.Tracks.empty()) clips.push_back(std::move(clip));
            }

            for (const auto& clip : clips) {
                LOOM_CORE_TRACE("MeshAsset: '{}' clip '{}' duration={:.3f}s tracks={}",
                                path, clip.Name, clip.Duration, clip.Tracks.size());
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
            { ShaderDataType::Float4, "a_Tangent"  },
        });

        auto ibo = IndexBuffer::Create(indices.data(), (uint32_t)indices.size());

        auto vao = VertexArray::Create();
        vao->AddVertexBuffer(vbo);

        // Skin VBO — only attached when the mesh has skinning data. Layout
        // claims attribute slots 4 (ivec4 joints) and 5 (vec4 weights),
        // matching mesh_skinned.vert. Static meshes leave these slots
        // disabled and use mesh.vert.
        if (any_skin && !skeleton.Empty()) {
            // Clamp joint indices to the skeleton's actual joint count so a
            // bogus index can't index past the bones UBO.
            const int max_joint = skeleton.JointCount() - 1;
            for (auto& sv : skin_attribs) {
                sv.Joints.x = std::clamp(sv.Joints.x, 0, max_joint);
                sv.Joints.y = std::clamp(sv.Joints.y, 0, max_joint);
                sv.Joints.z = std::clamp(sv.Joints.z, 0, max_joint);
                sv.Joints.w = std::clamp(sv.Joints.w, 0, max_joint);
            }
            auto skin_vbo = VertexBuffer::Create(skin_attribs.data(),
                                                 (uint32_t)(skin_attribs.size() * sizeof(SkinVertex)));
            skin_vbo->SetLayout({
                { ShaderDataType::Int4,   "a_Joints"  },
                { ShaderDataType::Float4, "a_Weights" },
            });
            vao->AddVertexBuffer(skin_vbo);
        }

        vao->SetIndexBuffer(ibo);

        auto asset = std::make_shared<MeshAsset>();
        asset->mVertexArray = std::move(vao);
        asset->mPath        = path;
        asset->mVertexCount = (uint32_t)vertices.size();
        asset->mIndexCount  = (uint32_t)indices.size();
        asset->mMaterial    = material;
        if (any_skin) asset->mSkeleton = std::move(skeleton);
        asset->mClips       = std::move(clips);

        LOOM_CORE_TRACE("MeshAsset: loaded '{}' ({} vertices, {} indices, normals={}, uvs={}, material={}, skin={}, clips={})",
                        path, asset->mVertexCount, asset->mIndexCount,
                        any_normals ? "yes" : "NO (defaulted to +Z)",
                        any_uvs     ? "yes" : "NO (tangents generated from geometry only; normal map will be flat)",
                        material.HasMaterial ? "yes" : "none",
                        asset->IsSkinned() ? std::to_string(asset->mSkeleton.JointCount()) + " joints" : "static",
                        asset->mClips.size());
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
        mSkeleton    = std::move(fresh->mSkeleton);
        mClips       = std::move(fresh->mClips);
        LOOM_CORE_INFO("MeshAsset: hot-reloaded '{}'", mPath);
    }

} // namespace Loom
