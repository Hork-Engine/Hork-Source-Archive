/*

Hork Engine Source Code

MIT License

Copyright (C) 2017-2022 Alexander Samusev.

This file is part of the Hork Engine Source Code.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "AssetImporter.h"
#include "Asset.h"
#include "ResourceManager.h"
#include "Engine.h"
#include <Core/Guid.h>
#include <Image/Image.h>
#include <Image/ImageEncoders.h>
#include <Platform/Logger.h>
#include <Platform/Memory/LinearAllocator.h>

#include <cgltf/cgltf.h>

static void unpack_vec2_or_vec3(cgltf_accessor* acc, Float3* output, size_t stride)
{
    int   num_elements;
    float position[3];

    if (!acc)
    {
        return;
    }

    if (acc->type == cgltf_type_vec2)
    {
        num_elements = 2;
    }
    else if (acc->type == cgltf_type_vec3)
    {
        num_elements = 3;
    }
    else
    {
        return;
    }

    position[2] = 0;

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, position, num_elements);

        Platform::Memcpy(ptr, position, sizeof(float) * 3);

        ptr += stride;
    }
}

static void unpack_vec2_or_vec3_to_half3(cgltf_accessor* acc, Half* output, size_t stride, bool normalize)
{
    int    num_elements;
    Float3 tmp;

    if (!acc)
    {
        return;
    }

    if (acc->type == cgltf_type_vec2)
    {
        num_elements = 2;
    }
    else if (acc->type == cgltf_type_vec3)
    {
        num_elements = 3;
    }
    else
    {
        return;
    }

    tmp[2] = 0;

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, tmp.ToPtr(), num_elements);

        if (normalize)
        {
            tmp.NormalizeSelf();
        }

        ((Half*)ptr)[0] = tmp[0];
        ((Half*)ptr)[1] = tmp[1];
        ((Half*)ptr)[2] = tmp[2];

        ptr += stride;
    }
}

static void unpack_vec2(cgltf_accessor* acc, Float2* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_vec2)
    {
        return;
    }

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, (float*)ptr, 2);

        ptr += stride;
    }
}

static void unpack_vec2_to_half2(cgltf_accessor* acc, Half* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_vec2)
    {
        return;
    }

    byte* ptr = (byte*)output;
    float tmp[2];

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, tmp, 2);

        ((Half*)ptr)[0] = tmp[0];
        ((Half*)ptr)[1] = tmp[1];

        ptr += stride;
    }
}

static void unpack_vec3(cgltf_accessor* acc, Float3* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_vec3)
    {
        return;
    }

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, (float*)ptr, 3);

        ptr += stride;
    }
}

static void unpack_vec4(cgltf_accessor* acc, Float4* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_vec4)
    {
        return;
    }

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, (float*)ptr, 4);

        ptr += stride;
    }
}

static void unpack_tangents(cgltf_accessor* acc, SMeshVertex* output)
{
    if (!acc || acc->type != cgltf_type_vec4)
    {
        return;
    }

    Float4 tmp;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, tmp.ToPtr(), 4);

        output->SetTangent(tmp.X, tmp.Y, tmp.Z);
        output->Handedness = tmp.W > 0.0f ? 1 : -1;

        output++;
    }
}

static void unpack_quat(cgltf_accessor* acc, Quat* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_vec4)
    {
        return;
    }

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, (float*)ptr, 4);

        ptr += stride;
    }
}

static void unpack_mat4(cgltf_accessor* acc, Float4x4* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_mat4)
    {
        return;
    }

    byte* ptr = (byte*)output;

    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, (float*)ptr, 16);

        ptr += stride;
    }
}

static void unpack_mat4_to_mat3x4(cgltf_accessor* acc, Float3x4* output, size_t stride)
{
    if (!acc || acc->type != cgltf_type_mat4)
    {
        return;
    }

    byte* ptr = (byte*)output;

    Float4x4 temp;
    for (int i = 0; i < acc->count; i++)
    {
        cgltf_accessor_read_float(acc, i, (float*)temp.ToPtr(), 16);

        Platform::Memcpy(ptr, temp.Transposed().ToPtr(), sizeof(Float3x4));

        ptr += stride;
    }
}

static void unpack_weights(cgltf_accessor* acc, SMeshVertexSkin* weights)
{
    float weight[4];

    if (!acc || acc->type != cgltf_type_vec4)
    {
        return;
    }

    for (int i = 0; i < acc->count; i++, weights++)
    {
        cgltf_accessor_read_float(acc, i, weight, 4);

        const float invSum = 255.0f / (weight[0] + weight[1] + weight[2] + weight[3]);

        weights->JointWeights[0] = Math::Clamp<int>(weight[0] * invSum, 0, 255);
        weights->JointWeights[1] = Math::Clamp<int>(weight[1] * invSum, 0, 255);
        weights->JointWeights[2] = Math::Clamp<int>(weight[2] * invSum, 0, 255);
        weights->JointWeights[3] = Math::Clamp<int>(weight[3] * invSum, 0, 255);
    }
}

static void unpack_joints(cgltf_accessor* acc, SMeshVertexSkin* weights)
{
    float indices[4];

    if (!acc || acc->type != cgltf_type_vec4)
    {
        return;
    }

    for (int i = 0; i < acc->count; i++, weights++)
    {
        cgltf_accessor_read_float(acc, i, indices, 4);

        weights->JointIndices[0] = Math::Clamp<int>(indices[0], 0, ASkeleton::MAX_JOINTS);
        weights->JointIndices[1] = Math::Clamp<int>(indices[1], 0, ASkeleton::MAX_JOINTS);
        weights->JointIndices[2] = Math::Clamp<int>(indices[2], 0, ASkeleton::MAX_JOINTS);
        weights->JointIndices[3] = Math::Clamp<int>(indices[3], 0, ASkeleton::MAX_JOINTS);
    }
}

static void sample_vec3(cgltf_animation_sampler* sampler, float frameTime, Float3& vec)
{
    cgltf_accessor* animtimes = sampler->input;
    cgltf_accessor* animdata  = sampler->output;

    float ft0, ftN, ct, nt;

    HK_ASSERT(animtimes->count > 0);

    cgltf_accessor_read_float(animtimes, 0, &ft0, 1);

    if (animtimes->count == 1 || frameTime <= ft0)
    {

        if (sampler->interpolation == cgltf_interpolation_type_cubic_spline)
        {
            cgltf_accessor_read_float(animdata, 0 * 3 + 1, (float*)vec.ToPtr(), 3);
        }
        else
        {
            cgltf_accessor_read_float(animdata, 0, (float*)vec.ToPtr(), 3);
        }

        return;
    }

    cgltf_accessor_read_float(animtimes, animtimes->count - 1, &ftN, 1);

    if (frameTime >= ftN)
    {

        if (sampler->interpolation == cgltf_interpolation_type_cubic_spline)
        {
            cgltf_accessor_read_float(animdata, (animtimes->count - 1) * 3 + 1, (float*)vec.ToPtr(), 3);
        }
        else
        {
            cgltf_accessor_read_float(animdata, animtimes->count - 1, (float*)vec.ToPtr(), 3);
        }

        return;
    }

    ct = ft0;

    for (int t = 0; t < (int)animtimes->count - 1; t++, ct = nt)
    {

        cgltf_accessor_read_float(animtimes, t + 1, &nt, 1);

        if (ct <= frameTime && nt > frameTime)
        {
            if (sampler->interpolation == cgltf_interpolation_type_linear)
            {
                if (frameTime == ct)
                {
                    cgltf_accessor_read_float(animdata, t, (float*)vec.ToPtr(), 3);
                }
                else
                {
                    Float3 p0, p1;
                    cgltf_accessor_read_float(animdata, t, (float*)p0.ToPtr(), 3);
                    cgltf_accessor_read_float(animdata, t + 1, (float*)p1.ToPtr(), 3);
                    float dur   = nt - ct;
                    float fract = (frameTime - ct) / dur;
                    HK_ASSERT(fract >= 0.0f && fract <= 1.0f);
                    vec = Math::Lerp(p0, p1, fract);
                }
            }
            else if (sampler->interpolation == cgltf_interpolation_type_step)
            {
                cgltf_accessor_read_float(animdata, t, (float*)vec.ToPtr(), 3);
            }
            else if (sampler->interpolation == cgltf_interpolation_type_cubic_spline)
            {
                float dur   = nt - ct;
                float fract = (dur == 0.0f) ? 0.0f : (frameTime - ct) / dur;
                HK_ASSERT(fract >= 0.0f && fract <= 1.0f);

                Float3 p0, m0, m1, p1;

                cgltf_accessor_read_float(animdata, t * 3 + 1, (float*)p0.ToPtr(), 3);
                cgltf_accessor_read_float(animdata, t * 3 + 2, (float*)m0.ToPtr(), 3);
                cgltf_accessor_read_float(animdata, (t + 1) * 3, (float*)m1.ToPtr(), 3);
                cgltf_accessor_read_float(animdata, (t + 1) * 3 + 1, (float*)p1.ToPtr(), 3);

                m0 *= dur;
                m1 *= dur;

                vec = Math::HermiteCubicSpline(p0, m0, p1, m1, fract);
            }
            break;
        }
    }
}

static void sample_quat(cgltf_animation_sampler* sampler, float frameTime, Quat& q)
{
    cgltf_accessor* animtimes = sampler->input;
    cgltf_accessor* animdata  = sampler->output;

    float ft0, ftN, ct, nt;

    HK_ASSERT(animtimes->count > 0);

    cgltf_accessor_read_float(animtimes, 0, &ft0, 1);

    if (animtimes->count == 1 || frameTime <= ft0)
    {

        if (sampler->interpolation == cgltf_interpolation_type_cubic_spline)
        {
            cgltf_accessor_read_float(animdata, 0 * 3 + 1, (float*)q.ToPtr(), 4);
        }
        else
        {
            cgltf_accessor_read_float(animdata, 0, (float*)q.ToPtr(), 4);
        }

        return;
    }

    cgltf_accessor_read_float(animtimes, animtimes->count - 1, &ftN, 1);

    if (frameTime >= ftN)
    {

        if (sampler->interpolation == cgltf_interpolation_type_cubic_spline)
        {
            cgltf_accessor_read_float(animdata, (animtimes->count - 1) * 3 + 1, (float*)q.ToPtr(), 4);
        }
        else
        {
            cgltf_accessor_read_float(animdata, animtimes->count - 1, (float*)q.ToPtr(), 4);
        }

        return;
    }

    ct = ft0;

    for (int t = 0; t < (int)animtimes->count - 1; t++, ct = nt)
    {

        cgltf_accessor_read_float(animtimes, t + 1, &nt, 1);

        if (ct <= frameTime && nt > frameTime)
        {
            if (sampler->interpolation == cgltf_interpolation_type_linear)
            {
                if (frameTime == ct)
                {
                    cgltf_accessor_read_float(animdata, t, (float*)q.ToPtr(), 4);
                }
                else
                {
                    Quat p0, p1;
                    cgltf_accessor_read_float(animdata, t, (float*)p0.ToPtr(), 4);
                    cgltf_accessor_read_float(animdata, t + 1, (float*)p1.ToPtr(), 4);
                    float dur   = nt - ct;
                    float fract = (frameTime - ct) / dur;
                    HK_ASSERT(fract >= 0.0f && fract <= 1.0f);
                    q = Math::Slerp(p0, p1, fract).Normalized();
                }
            }
            else if (sampler->interpolation == cgltf_interpolation_type_step)
            {
                cgltf_accessor_read_float(animdata, t, (float*)q.ToPtr(), 4);
            }
            else if (sampler->interpolation == cgltf_interpolation_type_cubic_spline)
            {
                float dur   = nt - ct;
                float fract = (dur == 0.0f) ? 0.0f : (frameTime - ct) / dur;
                HK_ASSERT(fract >= 0.0f && fract <= 1.0f);

                Quat p0, m0, m1, p1;

                cgltf_accessor_read_float(animdata, t * 3 + 1, (float*)p0.ToPtr(), 4);
                cgltf_accessor_read_float(animdata, t * 3 + 2, (float*)m0.ToPtr(), 4);
                cgltf_accessor_read_float(animdata, (t + 1) * 3, (float*)m1.ToPtr(), 4);
                cgltf_accessor_read_float(animdata, (t + 1) * 3 + 1, (float*)p1.ToPtr(), 4);

                m0 *= dur;
                m1 *= dur;

                p0.NormalizeSelf();
                m0.NormalizeSelf();
                m1.NormalizeSelf();
                p1.NormalizeSelf();

                q = Math::HermiteCubicSpline(p0, m0, p1, m1, fract);
                q.NormalizeSelf();
            }
            break;
        }
    }
}

static const char* GetErrorString(cgltf_result code)
{
    switch (code)
    {
        case cgltf_result_success:
            return "No error";
        case cgltf_result_data_too_short:
            return "Data too short";
        case cgltf_result_unknown_format:
            return "Unknown format";
        case cgltf_result_invalid_json:
            return "Invalid json";
        case cgltf_result_invalid_gltf:
            return "Invalid gltf";
        case cgltf_result_invalid_options:
            return "Invalid options";
        case cgltf_result_file_not_found:
            return "File not found";
        case cgltf_result_io_error:
            return "IO error";
        case cgltf_result_out_of_memory:
            return "Out of memory";
        default:
            ;
    }

    return "Unknown error";
}

static bool IsChannelValid(cgltf_animation_channel* channel)
{
    cgltf_animation_sampler* sampler = channel->sampler;

    switch (channel->target_path)
    {
        case cgltf_animation_path_type_translation:
        case cgltf_animation_path_type_rotation:
        case cgltf_animation_path_type_scale:
            break;
        case cgltf_animation_path_type_weights:
            LOG("Warning: animation path weights is not supported yet\n");
            return false;
        default:
            LOG("Warning: unknown animation target path\n");
            return false;
    }

    switch (sampler->interpolation)
    {
        case cgltf_interpolation_type_linear:
        case cgltf_interpolation_type_step:
        case cgltf_interpolation_type_cubic_spline:
            break;
        default:
            LOG("Warning: unknown interpolation type\n");
            return false;
    }

    cgltf_accessor* animtimes = sampler->input;
    cgltf_accessor* animdata  = sampler->output;

    if (animtimes->count == 0)
    {
        LOG("Warning: empty channel data\n");
        return false;
    }

    if (sampler->interpolation == cgltf_interpolation_type_cubic_spline && animtimes->count != animdata->count * 3)
    {
        LOG("Warning: invalid channel data\n");
        return false;
    }
    else if (animtimes->count != animdata->count)
    {
        LOG("Warning: invalid channel data\n");
        return false;
    }

    return true;
}

bool AAssetImporter::ImportGLTF(SAssetImportSettings const& InSettings)
{
    bool           ret    = false;
    AString const& Source = InSettings.ImportFile;

    m_Settings = InSettings;

    m_Path = PathUtils::GetFilePath(InSettings.ImportFile);
    m_Path += "/";

    AFile f = AFile::OpenRead(Source);
    if (!f)
    {
        LOG("Couldn't open {}\n", Source);
        return false;
    }

    HeapBlob blob = f.AsBlob();

    constexpr int MAX_MEMORY_GLTF = 16 << 20;
    using ALinearAllocatorGLTF = TLinearAllocator<MAX_MEMORY_GLTF>;

    ALinearAllocatorGLTF allocator;

    cgltf_options options;

    Platform::ZeroMem(&options, sizeof(options));

    options.memory.alloc = [](void* user, cgltf_size size)
    {
        ALinearAllocatorGLTF& Allocator = *static_cast<ALinearAllocatorGLTF*>(user);
        return Allocator.Allocate(size);
    };
    options.memory.free = [](void* user, void* ptr)
    {};
    options.memory.user_data = &allocator;

    cgltf_data* data = NULL;

    cgltf_result result = cgltf_parse(&options, blob.GetData(), blob.Size(), &data);
    if (result != cgltf_result_success)
    {
        LOG("Couldn't load {} : {}\n", Source, GetErrorString(result));
        return false;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success)
    {
        LOG("Couldn't load {} : {}\n", Source, GetErrorString(result));
        return false;
    }

    result = cgltf_load_buffers(&options, data, m_Path.CStr());
    if (result != cgltf_result_success)
    {
        LOG("Couldn't load {} buffers : {}\n", Source, GetErrorString(result));
        return false;
    }

    ret = ReadGLTF(data);

    WriteAssets();

    return true;
}

void AAssetImporter::ReadSkeleton(cgltf_node* node, int parentIndex)
{
    SJoint&  joint = m_Joints.Add();
    Float4x4 localTransform;

    cgltf_node_transform_local(node, (float*)localTransform.ToPtr());
    joint.LocalTransform = Float3x4(localTransform.Transposed());

    if (node->name)
    {
        Platform::Strcpy(joint.Name, sizeof(joint.Name), node->name);
    }
    else
    {
        AString name = HK_FORMAT("unnamed_{}", m_Joints.Size() - 1);
        Platform::Strcpy(joint.Name, sizeof(joint.Name), name.CStr());
    }

    LOG("ReadSkeleton: {}\n", node->name ? node->name : "unnamed");

    joint.Parent = parentIndex;

    // HACK: store joint index at camera pointer
    node->camera = (cgltf_camera*)(size_t)m_Joints.Size();

    parentIndex = m_Joints.Size() - 1;

    for (int i = 0; i < node->children_count; i++)
    {
        ReadSkeleton(node->children[i], parentIndex);
    }
}

bool AAssetImporter::ReadGLTF(cgltf_data* Data)
{
    m_Data      = Data;
    m_bSkeletal = Data->skins_count > 0 && m_Settings.bImportSkinning;

    m_Vertices.Clear();
    m_Weights.Clear();
    m_Indices.Clear();
    m_Meshes.Clear();
    m_Animations.Clear();
    m_Textures.Clear();
    m_Materials.Clear();
    m_Joints.Clear();
    m_BindposeBounds.Clear();
    m_Skin.JointIndices.Clear();
    m_Skin.OffsetMatrices.Clear();

    LOG("{} scenes\n", Data->scenes_count);
    LOG("{} skins\n", Data->skins_count);
    LOG("{} meshes\n", Data->meshes_count);
    LOG("{} nodes\n", Data->nodes_count);
    LOG("{} cameras\n", Data->cameras_count);
    LOG("{} lights\n", Data->lights_count);
    LOG("{} materials\n", Data->materials_count);

    if (Data->extensions_used_count > 0)
    {
        LOG("Used extensions:\n");
        for (int i = 0; i < Data->extensions_used_count; i++)
        {
            LOG("    {}\n", Data->extensions_used[i]);
        }
    }

    if (Data->extensions_required_count > 0)
    {
        LOG("Required extensions:\n");
        for (int i = 0; i < Data->extensions_required_count; i++)
        {
            LOG("    {}\n", Data->extensions_required[i]);
        }
    }

    if (m_Settings.bImportTextures)
    {
        m_Textures.Resize(Data->images_count);
        for (int i = 0; i < Data->images_count; i++)
        {
            m_Textures[i].GUID.Generate();
            m_Textures[i].Image = &Data->images[i];
        }
    }

    if (m_Settings.bImportMaterials)
    {
        m_Materials.Resize(Data->materials_count);
        for (int i = 0; i < Data->materials_count; i++)
        {
            ReadMaterial(&Data->materials[i], m_Materials[i]);
        }
    }

    for (int i = 0; i < Data->scenes_count; i++)
    {
        cgltf_scene* scene = &Data->scene[i];

        LOG("Scene \"{}\" nodes {}\n", scene->name ? scene->name : "unnamed", scene->nodes_count);

        for (int n = 0; n < scene->nodes_count; n++)
        {
            cgltf_node* node = scene->nodes[n];

            ReadNode_r(node);
        }
    }

    if (m_bSkeletal)
    {
        if (Data->skins)
        {
            // FIXME: Only one skin per file supported now
            // TODO: for ( int i = 0; i < Data->skins_count; i++ ) {
            cgltf_skin* skin = &Data->skins[0];

            m_SkeletonGUID.Generate();

            m_Joints.Clear();
#if 0
            ReadSkeleton( skin->skeleton );
#else

            int rootsCount = 0;
            for (int n = 0; n < Data->nodes_count; n++)
            {
                if (!Data->nodes[n].parent)
                {
                    rootsCount++;
                }
            }

            int parentIndex = -1;

            if (rootsCount > 1)
            {
                // Add root node

                SJoint& joint = m_Joints.Add();

                joint.LocalTransform.SetIdentity();
                Platform::Strcpy(joint.Name, sizeof(joint.Name), "generated_root");

                joint.Parent = -1;

                parentIndex = 0;
            }

            for (int n = 0; n < Data->nodes_count; n++)
            {
                if (!Data->nodes[n].parent)
                {
                    ReadSkeleton(&Data->nodes[n], parentIndex);
                }
            }
#endif

            // Apply scaling by changing local joint position
            if (m_Settings.Scale != 1.0f)
            {
                Float3   transl, scale;
                Float3x3 rot;
                //Float3x4 scaleMatrix = Float3x4::Scale( Float3( m_Settings.Scale ) );
                for (int i = 0; i < m_Joints.Size(); i++)
                {
                    SJoint* joint = &m_Joints[i];

                    // Scale skeleton joints
                    joint->LocalTransform.DecomposeAll(transl, rot, scale);
                    joint->LocalTransform.Compose(transl * m_Settings.Scale, rot, scale);
                }
            }

            // Apply rotation to root node
            if (!m_Joints.IsEmpty())
            {
                Float3x4 rotation     = Float3x4(m_Settings.Rotation.ToMatrix3x3().Transposed());
                SJoint*  joint        = &m_Joints[0];
                joint->LocalTransform = rotation * joint->LocalTransform;
            }

            // Read skin
            m_Skin.JointIndices.Resize(m_Joints.Size());
            m_Skin.OffsetMatrices.Resize(m_Joints.Size());

            unpack_mat4_to_mat3x4(skin->inverse_bind_matrices, m_Skin.OffsetMatrices.ToPtr(), sizeof(m_Skin.OffsetMatrices[0]));

            Float3x4 scaleMatrix = Float3x4::Scale(Float3(m_Settings.Scale));

            Float3x4 rotationInverse = Float3x4(m_Settings.Rotation.ToMatrix3x3().Inversed().Transposed());

            for (int i = 0; i < skin->joints_count; i++)
            {
                cgltf_node* jointNode = skin->joints[i];

                // Scale offset matrix
                m_Skin.OffsetMatrices[i] = scaleMatrix * m_Skin.OffsetMatrices[i] * scaleMatrix.Inversed() * rotationInverse;

                // Map skin onto joints
                m_Skin.JointIndices[i] = -1;

                // HACK: get joint index from camera pointer
                int nodeIndex = jointNode->camera ? (size_t)jointNode->camera - 1 : m_Joints.Size();
                if (nodeIndex >= m_Joints.Size())
                {
                    LOG("Invalid skin\n");
                    m_Skin.JointIndices[i] = 0;
                }
                else
                {
                    m_Skin.JointIndices[i] = nodeIndex;
                }
            }

            for (int i = skin->joints_count; i < m_Joints.Size(); i++)
            {
                m_Skin.OffsetMatrices[i].SetIdentity();

                // Scale offset matrix
                m_Skin.OffsetMatrices[i] = scaleMatrix * m_Skin.OffsetMatrices[i] * scaleMatrix.Inversed() * rotationInverse;

                // Map skin onto joints
                m_Skin.JointIndices[i] = i;
            }

            for (auto& mesh : m_Meshes)
            {
                if (!mesh.bSkinned)
                {
                    int nodeIndex = mesh.Node->camera ? (size_t)mesh.Node->camera - 1 : 0;

                    for (int n = 0; n < mesh.VertexCount; n++)
                    {
                        m_Weights[mesh.BaseVertex + n].JointIndices[0] = nodeIndex;
                        m_Weights[mesh.BaseVertex + n].JointIndices[1] = 0;
                        m_Weights[mesh.BaseVertex + n].JointIndices[2] = 0;
                        m_Weights[mesh.BaseVertex + n].JointIndices[3] = 0;
                        m_Weights[mesh.BaseVertex + n].JointWeights[0] = 255;
                        m_Weights[mesh.BaseVertex + n].JointWeights[1] = 0;
                        m_Weights[mesh.BaseVertex + n].JointWeights[2] = 0;
                        m_Weights[mesh.BaseVertex + n].JointWeights[3] = 0;
                    }
                }
            }

            m_BindposeBounds = CalcBindposeBounds(m_Vertices.ToPtr(), m_Weights.ToPtr(), m_Vertices.Size(), &m_Skin, m_Joints.ToPtr(), m_Joints.Size());

            LOG("Total skeleton nodes {}\n", m_Joints.Size());
            LOG("Total skinned nodes {}\n", m_Skin.JointIndices.Size());
        }

        if (!m_Joints.IsEmpty() && m_Settings.bImportAnimations)
        {
            ReadAnimations(Data);
        }
    }

    return true;
}

#if 0
#    include <World/Public/MaterialGraph/MaterialGraph.h>

static ETextureFilter ChooseSamplerFilter( int MinFilter, int MagFilter ) {
    // TODO:...

    return TEXTURE_FILTER_MIPMAP_TRILINEAR;
}

static ETextureAddress ChooseSamplerWrap( int Wrap ) {
    // TODO:...

    return TEXTURE_ADDRESS_WRAP;
}
#endif

AAssetImporter::TextureInfo* AAssetImporter::FindTextureImage(cgltf_texture const* Texture)
{
    if (!Texture)
    {
        return nullptr;
    }
    for (TextureInfo& texInfo : m_Textures)
    {
        if (texInfo.Image == Texture->image)
        {
            return &texInfo;
        }
    }
    return nullptr;
}

void AAssetImporter::SetTextureProps(TextureInfo* Info, const char* Name, bool SRGB)
{
    if (Info)
    {
        Info->bSRGB = SRGB;

        if (!Info->Image->name || !*Info->Image->name)
        {
            Info->Image->name = const_cast<char*>(Name);
        }
    }
}

void AAssetImporter::ReadMaterial(cgltf_material* Material, MaterialInfo& Info)
{
    Info.GUID.Generate();
    Info.Material        = Material;
    Info.DefaultMaterial = "/Default/Materials/Unlit";
    Info.NumTextures     = 0;
    Platform::ZeroMem(Info.Uniforms, sizeof(Info.Uniforms));

    if (Material->unlit && m_Settings.bAllowUnlitMaterials)
    {
        switch (Material->alpha_mode)
        {
            case cgltf_alpha_mode_opaque:
                Info.DefaultMaterial = "/Default/Materials/Unlit";
                break;
            case cgltf_alpha_mode_mask:
                Info.DefaultMaterial = "/Default/Materials/UnlitMask";
                break;
            case cgltf_alpha_mode_blend:
                Info.DefaultMaterial = "/Default/Materials/UnlitOpacity";
                break;
        }

        Info.NumTextures       = 1;
        Info.DefaultTexture[0] = "/Default/Textures/BaseColorWhite";

        if (Material->has_pbr_metallic_roughness)
        {
            Info.Textures[0] = FindTextureImage(Material->pbr_metallic_roughness.base_color_texture.texture);
        }
        else if (Material->has_pbr_specular_glossiness)
        {
            Info.Textures[0] = FindTextureImage(Material->pbr_specular_glossiness.diffuse_texture.texture);
        }
        else
        {
            Info.Textures[0] = nullptr;
        }

        SetTextureProps(Info.Textures[0], "Texture_BaseColor", true);

        // TODO: create material graph

#if 0
        cgltf_texture_view * texView;

        if ( Material->has_pbr_metallic_roughness ) {
            texView = &Material->pbr_metallic_roughness.base_color_texture;
        } else if ( Material->has_pbr_specular_glossiness ) {
            texView = &Material->pbr_specular_glossiness.diffuse_texture;
        } else {
            texView = nullptr;
        }

        MGMaterialGraph * graph = CreateInstanceOf< MGMaterialGraph >();

        if ( texView ) {
            MGInTexCoord * inTexCoordBlock = graph->AddNode< MGInTexCoord >();

            MGVertexStage * materialVertexStage = graph->AddNode< MGVertexStage >();

            MGNextStageVariable * texCoord = materialVertexStage->AddNextStageVariable( "TexCoord", AT_Float2 );
            texCoord->Connect( inTexCoordBlock, "Value" );

            MGTextureSlot * diffuseTexture = graph->AddNode< MGTextureSlot >();

            cgltf_sampler * sampler = texView->texture->sampler;
            diffuseTexture->SamplerDesc.Filter = ChooseSamplerFilter( sampler->min_filter, sampler->mag_filter );
            diffuseTexture->SamplerDesc.AddressU = ChooseSamplerWrap( sampler->wrap_s );
            diffuseTexture->SamplerDesc.AddressV = ChooseSamplerWrap( sampler->wrap_t );

            MGTextureLoad * textureSampler = graph->AddNode< MGTextureLoad >();
            textureSampler->TexCoord->Connect( materialVertexStage, "TexCoord" );
            textureSampler->Texture->Connect( diffuseTexture, "Value" );

            MGFragmentStage * materialFragmentStage = graph->AddNode< MGFragmentStage >();
            materialFragmentStage->Color->Connect( textureSampler, "RGBA" );

            graph->VertexStage = materialVertexStage;
            graph->FragmentStage = materialFragmentStage;
            graph->MaterialType = MATERIAL_TYPE_UNLIT;
            graph->RegisterTextureSlot( diffuseTexture );

            Info.Graph = graph;
        } else {
            Info.Graph = nullptr; // Will be used default material
        }
#endif
    }
    else if (Material->has_pbr_metallic_roughness)
    {

        Info.NumTextures       = 5;
        Info.DefaultTexture[0] = "/Default/Textures/BaseColorWhite"; // base color
        Info.DefaultTexture[1] = "/Default/Textures/White";          // metallic&roughness
        Info.DefaultTexture[2] = "/Default/Textures/Normal";         // normal
        Info.DefaultTexture[3] = "/Default/Textures/White";          // occlusion
        Info.DefaultTexture[4] = "/Default/Textures/Black";          // emissive

        bool emissiveFactor = Material->emissive_factor[0] > 0.0f || Material->emissive_factor[1] > 0.0f || Material->emissive_factor[2] > 0.0f;

        bool factor = Material->pbr_metallic_roughness.base_color_factor[0] < 1.0f || Material->pbr_metallic_roughness.base_color_factor[1] < 1.0f || Material->pbr_metallic_roughness.base_color_factor[2] < 1.0f || Material->pbr_metallic_roughness.base_color_factor[3] < 1.0f || Material->pbr_metallic_roughness.metallic_factor < 1.0f || Material->pbr_metallic_roughness.roughness_factor < 1.0f || emissiveFactor;

        if (emissiveFactor)
        {
            Info.DefaultTexture[4] = "/Default/Textures/White"; // emissive
        }

        if (factor)
        {
            switch (Material->alpha_mode)
            {
                case cgltf_alpha_mode_opaque:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessFactor";
                    break;
                case cgltf_alpha_mode_mask:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessFactorMask";
                    break;
                case cgltf_alpha_mode_blend:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessFactorOpacity";
                    break;
            }

            Info.Uniforms[0] = Material->pbr_metallic_roughness.base_color_factor[0];
            Info.Uniforms[1] = Material->pbr_metallic_roughness.base_color_factor[1];
            Info.Uniforms[2] = Material->pbr_metallic_roughness.base_color_factor[2];
            Info.Uniforms[3] = Material->pbr_metallic_roughness.base_color_factor[3];

            Info.Uniforms[4] = Material->pbr_metallic_roughness.metallic_factor;
            Info.Uniforms[5] = Material->pbr_metallic_roughness.roughness_factor;
            Info.Uniforms[6] = 0;
            Info.Uniforms[7] = 0;

            Info.Uniforms[8]  = Material->emissive_factor[0];
            Info.Uniforms[9]  = Material->emissive_factor[1];
            Info.Uniforms[10] = Material->emissive_factor[2];
        }
        else
        {
            switch (Material->alpha_mode)
            {
                case cgltf_alpha_mode_opaque:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughness";
                    break;
                case cgltf_alpha_mode_mask:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessMask";
                    break;
                case cgltf_alpha_mode_blend:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessOpacity";
                    break;
            }
        }

        Info.Textures[0] = FindTextureImage(Material->pbr_metallic_roughness.base_color_texture.texture);
        Info.Textures[1] = FindTextureImage(Material->pbr_metallic_roughness.metallic_roughness_texture.texture);
        Info.Textures[2] = FindTextureImage(Material->normal_texture.texture);
        Info.Textures[3] = FindTextureImage(Material->occlusion_texture.texture);
        Info.Textures[4] = FindTextureImage(Material->emissive_texture.texture);

        SetTextureProps(Info.Textures[0], "Texture_BaseColor", true);
        SetTextureProps(Info.Textures[1], "Texture_MetallicRoughness", false);
        SetTextureProps(Info.Textures[2], "Texture_Normal", false);
        if (Info.Textures[3] != Info.Textures[1])
            SetTextureProps(Info.Textures[3], "Texture_Occlusion", true);
        SetTextureProps(Info.Textures[4], "Texture_Emissive", true);

        // TODO: create material graph

        //cgltf_pbr_metallic_roughness & tex = Material->pbr_metallic_roughness;

        // TODO: create pbr material
    }
    else if (Material->has_pbr_specular_glossiness)
    {
        LOG("Warning: pbr specular glossiness workflow is not supported yet\n");

        Info.NumTextures       = 5;
        Info.DefaultTexture[0] = "/Default/Textures/BaseColorWhite"; // diffuse
        Info.DefaultTexture[1] = "/Default/Textures/White";          // specular&glossiness
        Info.DefaultTexture[2] = "/Default/Textures/Normal";         // normal
        Info.DefaultTexture[3] = "/Default/Textures/White";          // occlusion
        Info.DefaultTexture[4] = "/Default/Textures/Black";          // emissive

        bool emissiveFactor = Material->emissive_factor[0] > 0.0f || Material->emissive_factor[1] > 0.0f || Material->emissive_factor[2] > 0.0f;

        bool factor = Material->pbr_specular_glossiness.diffuse_factor[0] < 1.0f || Material->pbr_specular_glossiness.diffuse_factor[1] < 1.0f || Material->pbr_specular_glossiness.diffuse_factor[2] < 1.0f || Material->pbr_specular_glossiness.diffuse_factor[3] < 1.0f || Material->pbr_specular_glossiness.specular_factor[0] < 1.0f || Material->pbr_specular_glossiness.glossiness_factor < 1.0f || emissiveFactor;

        if (emissiveFactor)
        {
            Info.DefaultTexture[4] = "/Default/Textures/White"; // emissive
        }

        if (factor)
        {
            switch (Material->alpha_mode)
            {
                case cgltf_alpha_mode_opaque:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessFactor";
                    break;
                case cgltf_alpha_mode_mask:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessFactorMask";
                    break;
                case cgltf_alpha_mode_blend:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessFactorOpacity";
                    break;
            }

            //Info.DefaultMaterial = "/Default/Materials/PBRSpecularGlossinessFactor";

            Info.Uniforms[0] = Material->pbr_specular_glossiness.diffuse_factor[0];
            Info.Uniforms[1] = Material->pbr_specular_glossiness.diffuse_factor[1];
            Info.Uniforms[2] = Material->pbr_specular_glossiness.diffuse_factor[2];
            Info.Uniforms[3] = Material->pbr_specular_glossiness.diffuse_factor[3];

            Info.Uniforms[4] = Material->pbr_specular_glossiness.specular_factor[0];
            Info.Uniforms[5] = Material->pbr_specular_glossiness.glossiness_factor;
            Info.Uniforms[6] = 0;
            Info.Uniforms[7] = 0;

            Info.Uniforms[8]  = Material->emissive_factor[0];
            Info.Uniforms[9]  = Material->emissive_factor[1];
            Info.Uniforms[10] = Material->emissive_factor[2];
        }
        else
        {
            switch (Material->alpha_mode)
            {
                case cgltf_alpha_mode_opaque:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughness";
                    break;
                case cgltf_alpha_mode_mask:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessMask";
                    break;
                case cgltf_alpha_mode_blend:
                    Info.DefaultMaterial = "/Default/Materials/PBRMetallicRoughnessOpacity";
                    break;
            }

            //Info.DefaultMaterial = "/Default/Materials/PBRSpecularGlossiness";
        }

        Info.Textures[0] = FindTextureImage(Material->pbr_specular_glossiness.diffuse_texture.texture);
        Info.Textures[1] = FindTextureImage(Material->pbr_specular_glossiness.specular_glossiness_texture.texture);
        Info.Textures[2] = FindTextureImage(Material->normal_texture.texture);
        Info.Textures[3] = FindTextureImage(Material->occlusion_texture.texture);
        Info.Textures[4] = FindTextureImage(Material->emissive_texture.texture);

        SetTextureProps(Info.Textures[0], "Texture_Diffuse", true);
        SetTextureProps(Info.Textures[1], "Texture_SpecularGlossiness", false);
        SetTextureProps(Info.Textures[2], "Texture_Normal", false);
        SetTextureProps(Info.Textures[3], "Texture_Occlusion", true);
        SetTextureProps(Info.Textures[4], "Texture_Emissive", true);

        //if (Material->alpha_mode == cgltf_alpha_mode_blend)
        //    SetTextureProps(Info.Textures[5], "Texture_Opacity", true);
    }
}

void AAssetImporter::ReadNode_r(cgltf_node* Node)
{

    //LOG( "node \"{}\"\n", Node->name );

    //if ( Node->camera ) {
    //    LOG( "has camera\n" );
    //}

    //if ( Node->light ) {
    //    LOG( "has light\n" );
    //}

    //if ( Node->weights ) {
    //    LOG( "has weights {}\n", Node->weights_count );
    //}

    if (m_Settings.bImportMeshes || m_Settings.bImportSkinning || m_Settings.bImportAnimations)
    {
        ReadMesh(Node);
    }

    for (int n = 0; n < Node->children_count; n++)
    {
        cgltf_node* child = Node->children[n];

        ReadNode_r(child);
    }
}

void AAssetImporter::ReadMesh(cgltf_node* Node)
{
    cgltf_mesh* mesh = Node->mesh;

    if (!mesh)
    {
        return;
    }

    Float3x4 globalTransform;
    Float3x3 normalMatrix;
    Float4x4 temp;
    cgltf_node_transform_world(Node, (float*)temp.ToPtr());
    Float3x4 rotation = Float3x4(m_Settings.Rotation.ToMatrix3x3().Transposed());
    globalTransform   = rotation * Float3x4(temp.Transposed());
    globalTransform.DecomposeNormalMatrix(normalMatrix);

    ReadMesh(Node, mesh, Float3x4::Scale(Float3(m_Settings.Scale)) * globalTransform, normalMatrix);
}

void AAssetImporter::ReadMesh(cgltf_node* Node, cgltf_mesh* Mesh, Float3x4 const& GlobalTransform, Float3x3 const& NormalMatrix)
{
    struct ASortFunction
    {
        bool operator()(cgltf_primitive const& _A, cgltf_primitive const& _B)
        {
            return (_A.material < _B.material);
        }
    } SortFunction;

    std::sort(&Mesh->primitives[0], &Mesh->primitives[Mesh->primitives_count], SortFunction);

    cgltf_material* material = nullptr;

    MeshInfo* meshInfo = nullptr;

    const Half pos = 1.0f;
    const Half zero = 0.0f;

    for (int i = 0; i < Mesh->primitives_count; i++)
    {
        cgltf_primitive* prim = &Mesh->primitives[i];

        if (prim->type != cgltf_primitive_type_triangles)
        {
            LOG("Only triangle primitives supported\n");
            continue;
        }

        cgltf_accessor* position = nullptr;
        cgltf_accessor* normal   = nullptr;
        cgltf_accessor* tangent  = nullptr;
        cgltf_accessor* texcoord = nullptr;
        cgltf_accessor* color    = nullptr;
        cgltf_accessor* joints   = nullptr;
        cgltf_accessor* weights  = nullptr;

        for (int a = 0; a < prim->attributes_count; a++)
        {
            cgltf_attribute* attrib = &prim->attributes[a];

            if (attrib->data->is_sparse)
            {
                LOG("Warning: sparsed accessors are not supported\n");
                continue;
            }

            switch (attrib->type)
            {
                case cgltf_attribute_type_invalid:
                    LOG("Warning: invalid attribute type\n");
                    continue;

                case cgltf_attribute_type_position:
                    position = attrib->data;
                    break;

                case cgltf_attribute_type_normal:
                    normal = attrib->data;
                    break;

                case cgltf_attribute_type_tangent:
                    tangent = attrib->data;
                    break;

                case cgltf_attribute_type_texcoord:
                    // get first texcoord channel
                    if (!texcoord)
                    {
                        texcoord = attrib->data;
                    }
                    break;

                case cgltf_attribute_type_color:
                    color = attrib->data;
                    break;

                case cgltf_attribute_type_joints:
                    joints = attrib->data;
                    break;

                case cgltf_attribute_type_weights:
                    weights = attrib->data;
                    break;
            }
        }

        if (!position)
        {
            LOG("Warning: no positions\n");
            continue;
        }

        if (position->type != cgltf_type_vec2 && position->type != cgltf_type_vec3)
        {
            LOG("Warning: invalid vertex positions\n");
            continue;
        }

        if (!texcoord)
        {
            LOG("Warning: no texcoords\n");
        }

        if (texcoord && texcoord->type != cgltf_type_vec2)
        {
            LOG("Warning: invalid texcoords\n");
            texcoord = nullptr;
        }

        int vertexCount = position->count;
        if (texcoord && texcoord->count != vertexCount)
        {
            LOG("Warning: texcoord count != position count\n");
            texcoord = nullptr;
        }

        if (!material || material != prim->material || !m_Settings.bMergePrimitives)
        {
            meshInfo = &m_Meshes.Add();
            meshInfo->GUID.Generate();
            meshInfo->BaseVertex  = m_Vertices.Size();
            meshInfo->FirstIndex  = m_Indices.Size();
            meshInfo->VertexCount = 0;
            meshInfo->IndexCount  = 0;
            meshInfo->Mesh        = Mesh;
            meshInfo->Material    = prim->material;
            meshInfo->BoundingBox.Clear();

            meshInfo->Node      = Node;
            meshInfo->bSkinned = weights != nullptr;

            material = prim->material;
        }

        int firstVert = m_Vertices.Size();
        m_Vertices.Resize(firstVert + vertexCount);

        int vertexOffset = firstVert - meshInfo->BaseVertex;

        int firstIndex = m_Indices.Size();
        int indexCount;
        if (prim->indices)
        {
            indexCount = prim->indices->count;
            m_Indices.Resize(firstIndex + indexCount);
            unsigned int* pInd = m_Indices.ToPtr() + firstIndex;
            for (int index = 0; index < indexCount; index++, pInd++)
            {
                *pInd = vertexOffset + cgltf_accessor_read_index(prim->indices, index);
            }
        }
        else
        {
            indexCount = vertexCount;
            m_Indices.Resize(firstIndex + indexCount);
            unsigned int* pInd = m_Indices.ToPtr() + firstIndex;
            for (int index = 0; index < indexCount; index++, pInd++)
            {
                *pInd = vertexOffset + index;
            }
        }

        unpack_vec2_or_vec3(position, &m_Vertices[firstVert].Position, sizeof(SMeshVertex));

        if (texcoord)
        {
            unpack_vec2_to_half2(texcoord, &m_Vertices[firstVert].TexCoord[0], sizeof(SMeshVertex));
        }
        else
        {
            for (int v = 0; v < vertexCount; v++)
            {
                m_Vertices[firstVert + v].SetTexCoord(zero, zero);
            }
        }

        if (normal && (normal->type == cgltf_type_vec2 || normal->type == cgltf_type_vec3) && normal->count == vertexCount)
        {
            unpack_vec2_or_vec3_to_half3(normal, &m_Vertices[firstVert].Normal[0], sizeof(SMeshVertex), true);
        }
        else
        {
            // TODO: compute normals

            LOG("Warning: no normals\n");

            for (int v = 0; v < vertexCount; v++)
            {
                m_Vertices[firstVert + v].SetNormal(zero, pos, zero);
            }
        }

        if (tangent && (tangent->type == cgltf_type_vec4) && tangent->count == vertexCount)
        {
            unpack_tangents(tangent, &m_Vertices[firstVert]);

            //CalcTangentSpace( m_Vertices.ToPtr() + meshInfo->BaseVertex, m_Vertices.Size() - meshInfo->BaseVertex, m_Indices.ToPtr() + firstIndex, indexCount );
        }
        else
        {

            if (texcoord)
            {
                CalcTangentSpace(m_Vertices.ToPtr() + meshInfo->BaseVertex, m_Vertices.Size() - meshInfo->BaseVertex, m_Indices.ToPtr() + firstIndex, indexCount);
            }
            else
            {
                SMeshVertex* pVert = m_Vertices.ToPtr() + firstVert;
                for (int v = 0; v < vertexCount; v++, pVert++)
                {
                    pVert->SetTangent(pos, zero, zero);
                    pVert->Handedness = 1;
                }
            }
        }

        if (weights && (weights->type == cgltf_type_vec4) && weights->count == vertexCount && joints && (joints->type == cgltf_type_vec4) && joints->count == vertexCount)
        {
            m_Weights.Resize(m_Vertices.Size());
            unpack_weights(weights, &m_Weights[firstVert]);
            unpack_joints(joints, &m_Weights[firstVert]);
        }

        HK_UNUSED(color);

        SMeshVertex* pVert = m_Vertices.ToPtr() + firstVert;

        if (/*m_Settings.bSingleModel && */ !m_bSkeletal)
        {
            for (int v = 0; v < vertexCount; v++, pVert++)
            {
                // Pretransform vertices
                pVert->Position = Float3(GlobalTransform * pVert->Position);
                pVert->SetNormal(NormalMatrix * pVert->GetNormal());
                pVert->SetTangent(NormalMatrix * pVert->GetTangent());

                // Calc bounding box
                meshInfo->BoundingBox.AddPoint(pVert->Position);
            }
        }
        else
        {
            Float3x3 rotation = m_Settings.Rotation.ToMatrix3x3();
            for (int v = 0; v < vertexCount; v++, pVert++)
            {
                pVert->Position = m_Settings.Scale * Float3(rotation * pVert->Position);
                pVert->SetNormal(rotation * pVert->GetNormal());
                pVert->SetTangent(rotation * pVert->GetTangent());

                // Calc bounding box
                meshInfo->BoundingBox.AddPoint(pVert->Position);
            }
        }

        meshInfo->VertexCount += vertexCount;
        meshInfo->IndexCount += indexCount;

        //cgltf_morph_target* targets;
        //cgltf_size targets_count;
    }

    //for ( int i = 0; i < mesh->weights_count; i++ ) {
    //    cgltf_float * w = &mesh->weights[ i ];
    //}

    LOG("Subparts {}, Primitives {}\n", m_Meshes.Size(), Mesh->primitives_count);

    if (m_bSkeletal)
    {
        int numWeights  = m_Weights.Size();
        int numVertices = m_Vertices.Size();
        if (numWeights != numVertices)
        {
            LOG("Warning: invalid mesh (num weights != num vertices)\n");

            m_Weights.Resize(numVertices);

            // Clear
            int count = numVertices - numWeights;
            for (int i = 0; i < count; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    m_Weights[numWeights + i].JointIndices[j] = 0;
                    m_Weights[numWeights + i].JointWeights[j] = 0;
                }
                m_Weights[numWeights + i].JointWeights[0] = 255;
            }
        }
    }
}

void AAssetImporter::ReadAnimations(cgltf_data* Data)
{
    m_Animations.Resize(Data->animations_count);
    for (int animIndex = 0; animIndex < Data->animations_count; animIndex++)
    {
        AnimationInfo& animation = m_Animations[animIndex];

        ReadAnimation(&Data->animations[animIndex], animation);

        CalcBoundingBoxes(m_Vertices.ToPtr(),
                          m_Weights.ToPtr(),
                          m_Vertices.Size(),
                          &m_Skin,
                          m_Joints.ToPtr(),
                          m_Joints.Size(),
                          animation.FrameCount,
                          animation.Channels.ToPtr(),
                          animation.Channels.Size(),
                          animation.Transforms.ToPtr(),
                          animation.Bounds);
    }
}

void AAssetImporter::ReadAnimation(cgltf_animation* Anim, AnimationInfo& Animation)
{
    const int framesPerSecond = 30;
    //    float gcd = 0;
    float maxDuration = 0;

    for (int ch = 0; ch < Anim->channels_count; ch++)
    {
        cgltf_animation_channel* channel   = &Anim->channels[ch];
        cgltf_animation_sampler* sampler   = channel->sampler;
        cgltf_accessor*          animtimes = sampler->input;

        if (animtimes->count == 0)
        {
            continue;
        }

#if 0
        float time = 0;
        for ( int t = 0; t < animtimes->count; t++ ) {
            cgltf_accessor_read_float( animtimes, t, &time, 1 );
            gcd = Math::GreaterCommonDivisor( gcd, time );
        }
#endif
        float time = 0;
        cgltf_accessor_read_float(animtimes, animtimes->count - 1, &time, 1);
        maxDuration = Math::Max(maxDuration, time);
    }

    int numFrames = maxDuration * framesPerSecond;
    //numFrames--;

    float frameDelta = maxDuration / numFrames;

    Animation.GUID.Generate();
    Animation.Name       = Anim->name ? Anim->name : "Animation";
    Animation.FrameDelta = frameDelta;
    Animation.FrameCount = numFrames; // frames count, animation duration is FrameDelta * ( FrameCount - 1 )

    for (int ch = 0; ch < Anim->channels_count; ch++)
    {

        cgltf_animation_channel* channel = &Anim->channels[ch];
        cgltf_animation_sampler* sampler = channel->sampler;

        if (!IsChannelValid(channel))
        {
            continue;
        }

        // HACK: get joint index from camera pointer
        int nodeIndex = channel->target_node->camera ? (size_t)channel->target_node->camera - 1 : m_Joints.Size();
        if (nodeIndex >= m_Joints.Size())
        {
            LOG("Warning: joint {} is not found\n", channel->target_node->name);
            continue;
        }

        // just for debug
        //if ( channel->target_node->name && Core::Icmp( channel->target_node->name, "L_arm_029" ) ) {
        //    continue;
        //}

        int mergedChannel;
        for (mergedChannel = 0; mergedChannel < Animation.Channels.Size(); mergedChannel++)
        {
            if (nodeIndex == Animation.Channels[mergedChannel].JointIndex)
            {
                break;
            }
        }

        SAnimationChannel* jointAnim;

        if (mergedChannel < Animation.Channels.Size())
        {
            jointAnim = &Animation.Channels[mergedChannel];
        }
        else
        {
            jointAnim                  = &Animation.Channels.Add();
            jointAnim->JointIndex      = nodeIndex;
            jointAnim->TransformOffset = Animation.Transforms.Size();
            jointAnim->bHasPosition    = false;
            jointAnim->bHasRotation    = false;
            jointAnim->bHasScale       = false;
            Animation.Transforms.Resize(Animation.Transforms.Size() + numFrames);

            Float3   position;
            Float3x3 rotation;
            Quat     q;
            Float3   scale;
            m_Joints[nodeIndex].LocalTransform.DecomposeAll(position, rotation, scale);
            q.FromMatrix(rotation);
            for (int f = 0; f < numFrames; f++)
            {
                STransform& transform = Animation.Transforms[jointAnim->TransformOffset + f];
                transform.Position    = position;
                transform.Scale       = scale;
                transform.Rotation    = q;
            }
        }

        switch (channel->target_path)
        {
            case cgltf_animation_path_type_translation:
                jointAnim->bHasPosition = true;

                for (int f = 0; f < numFrames; f++)
                {
                    STransform& transform = Animation.Transforms[jointAnim->TransformOffset + f];

                    sample_vec3(sampler, f * frameDelta, transform.Position);

                    transform.Position *= m_Settings.Scale;
                }

                break;
            case cgltf_animation_path_type_rotation:
                jointAnim->bHasRotation = true;

                for (int f = 0; f < numFrames; f++)
                {
                    STransform& transform = Animation.Transforms[jointAnim->TransformOffset + f];

                    sample_quat(sampler, f * frameDelta, transform.Rotation);
                }

                break;
            case cgltf_animation_path_type_scale:
                jointAnim->bHasScale = true;

                for (int f = 0; f < numFrames; f++)
                {
                    STransform& transform = Animation.Transforms[jointAnim->TransformOffset + f];

                    sample_vec3(sampler, f * frameDelta, transform.Scale);
                }

                break;
            default:
                LOG("Warning: Unsupported target path\n");
                break;
        }

        for (int f = 0; f < numFrames; f++)
        {
            STransform& transform = Animation.Transforms[jointAnim->TransformOffset + f];

            float frameTime = f * frameDelta;

            switch (channel->target_path)
            {
                case cgltf_animation_path_type_translation:
                    sample_vec3(sampler, frameTime, transform.Position);
                    transform.Position *= m_Settings.Scale;
                    break;
                case cgltf_animation_path_type_rotation:
                    sample_quat(sampler, frameTime, transform.Rotation);
                    break;
                case cgltf_animation_path_type_scale:
                    sample_vec3(sampler, frameTime, transform.Scale);
                    break;
                default:
                    LOG("Warning: Unsupported target path\n");
                    f = numFrames;
                    break;
            }
        }
    }

    for (int channel = 0; channel < Animation.Channels.Size(); channel++)
    {
        SAnimationChannel* jointAnim = &Animation.Channels[channel];

        if (jointAnim->JointIndex == 0 && jointAnim->bHasRotation)
        {
            for (int frameIndex = 0; frameIndex < numFrames; frameIndex++)
            {

                STransform& transform = Animation.Transforms[jointAnim->TransformOffset + frameIndex];

                transform.Rotation = m_Settings.Rotation * transform.Rotation;
            }
        }
    }
}

#if 0
Float3x4 CalcJointWorldTransform( SJoint const & InJoint, SJoint const * InJoints ) {
    if ( InJoint.Parent == -1 ) {
        return InJoint.LocalTransform;
    }
    return CalcJointWorldTransform( InJoints[InJoint.Parent], InJoints ) * InJoint.LocalTransform;
}
#endif

void AAssetImporter::WriteAssets()
{
    if (m_Settings.bImportTextures)
    {
        WriteTextures();
    }

    if (m_Settings.bImportMaterials)
    {
        WriteMaterials();
    }

    if (m_Settings.bImportSkinning)
    {
        if (m_Settings.bImportSkeleton)
        {
            WriteSkeleton();
        }

        if (m_Settings.bImportAnimations)
        {
            WriteAnimations();
        }
    }

    if (m_Settings.bImportMeshes)
    {
        if (m_Settings.bSingleModel || m_bSkeletal)
        {
            WriteSingleModel();
        }
        else
        {
            WriteMeshes();
        }
    }
}

void AAssetImporter::WriteTextures()
{
    for (TextureInfo& tex : m_Textures)
    {
        WriteTexture(tex);
    }
}

void AAssetImporter::WriteTexture(TextureInfo& tex)
{
    AString     fileName       = GeneratePhysicalPath(tex.Image->name && *tex.Image->name ? tex.Image->name : "texture", ".texture");
    AString     sourceFileName = m_Path + tex.Image->uri;
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    ImageMipmapConfig mipmapConfig;
    mipmapConfig.EdgeMode = IMAGE_RESAMPLE_EDGE_WRAP;
    mipmapConfig.Filter   = IMAGE_RESAMPLE_FILTER_MITCHELL;

    ImageStorage image = CreateImage(sourceFileName, &mipmapConfig, IMAGE_STORAGE_FLAGS_DEFAULT, tex.bSRGB ? TEXTURE_FORMAT_SRGBA8_UNORM : TEXTURE_FORMAT_RGBA8_UNORM);
    //ImageStorage image = CreateImage(sourceFileName, &mipmapConfig, IMAGE_STORAGE_FLAGS_DEFAULT, tex.bSRGB ? TEXTURE_FORMAT_BC6H_UFLOAT : TEXTURE_FORMAT_RGBA8_UNORM);
    if (!image)
        return;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    GuidMap[tex.GUID] = "/Root/" + fileName;

    f.WriteUInt32(FMT_FILE_TYPE_TEXTURE);
    f.WriteUInt32(FMT_VERSION_TEXTURE);
    f.WriteObject(image);

    f.WriteUInt32(1); // num source files
    f.WriteString(sourceFileName);
}

void AAssetImporter::WriteMaterials()
{
    for (MaterialInfo& m : m_Materials)
    {
        WriteMaterial(m);
    }
}

void AAssetImporter::WriteMaterial(MaterialInfo const& m)
{
    AString     fileName       = GeneratePhysicalPath("matinst", ".minst");
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    GuidMap[m.GUID] = "/Root/" + fileName;

    f.FormattedPrint("Material \"{}\"\n", m.DefaultMaterial);
    f.FormattedPrint("Textures [\n");
    for (int i = 0; i < m.NumTextures; i++)
    {
        if (m.Textures[i])
        {
            f.FormattedPrint("\"{}\"\n", GuidMap[m.Textures[i]->GUID]);
        }
        else
        {
            f.FormattedPrint("\"{}\"\n", m.DefaultTexture[i]);
        }
    }
    f.FormattedPrint("]\n");
    f.FormattedPrint("Uniforms [\n");
    for (int i = 0; i < MAX_MATERIAL_UNIFORMS; i++)
    {
        f.FormattedPrint("\"{}\"\n", Core::ToString(m.Uniforms[i]));
    }
    f.FormattedPrint("]\n");
}

static AString ValidateFileName(AStringView FileName)
{
    AString ValidatedName = FileName;

    for (StringSizeType i = 0; i < ValidatedName.Size(); i++)
    {
        char& Ch = ValidatedName[i];

        switch (Ch)
        {
            case ':':
            case '\\':
            case '/':
            case '?':
            case '@':
            case '$':
            case '*':
            case '|':
                Ch = '_';
                break;
        }
    }

    return ValidatedName;
}

AString AAssetImporter::GeneratePhysicalPath(AStringView DesiredName, AStringView Extension)
{
    AString sourceName = PathUtils::GetFilenameNoExt(PathUtils::GetFilenameNoPath(m_Settings.ImportFile));
    AString validatedName = ValidateFileName(DesiredName);

    sourceName.ToLower();
    validatedName.ToLower();

    AString path   = m_Settings.OutputPath / sourceName + "_" + validatedName;
    AString result = path + Extension;

    int uniqueNumber = 0;

    while (Core::IsFileExists(GEngine->GetRootPath() + result))
    {
        result = path + "_" + Core::ToString(++uniqueNumber) + Extension;
    }

    return result;
}

AGUID AAssetImporter::GetMaterialGUID(cgltf_material* Material)
{
    for (MaterialInfo& m : m_Materials)
    {
        if (m.Material == Material)
        {
            return m.GUID;
        }
    }
    return {};
}

void AAssetImporter::WriteSkeleton()
{
    if (!m_Joints.IsEmpty())
    {
        AString     fileName       = GeneratePhysicalPath("skeleton", ".skeleton");
        AString     fileSystemPath = GEngine->GetRootPath() + fileName;

        AFile f = AFile::OpenWrite(fileSystemPath);
        if (!f)
        {
            LOG("Failed to write {}\n", fileName);
            return;
        }

        GuidMap[m_SkeletonGUID] = "/Root/" + fileName;

        f.WriteUInt32(FMT_FILE_TYPE_SKELETON);
        f.WriteUInt32(FMT_VERSION_SKELETON);
        f.WriteString(m_SkeletonGUID.ToString());
        f.WriteArray(m_Joints);
        f.WriteObject(m_BindposeBounds);
    }
}

void AAssetImporter::WriteAnimations()
{
    for (AnimationInfo& animation : m_Animations)
    {
        WriteAnimation(animation);
    }
}

void AAssetImporter::WriteAnimation(AnimationInfo const& Animation)
{
    AString     fileName       = GeneratePhysicalPath(Animation.Name, ".animation");
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    f.WriteUInt32(FMT_FILE_TYPE_ANIMATION);
    f.WriteUInt32(FMT_VERSION_ANIMATION);
    f.WriteString(Animation.GUID.ToString());
    f.WriteFloat(Animation.FrameDelta);
    f.WriteUInt32(Animation.FrameCount);
    f.WriteArray(Animation.Channels);
    f.WriteArray(Animation.Transforms);
    f.WriteArray(Animation.Bounds);
}

void AAssetImporter::WriteSingleModel()
{
    if (m_Meshes.IsEmpty())
    {
        return;
    }

    AString     fileName       = GeneratePhysicalPath("mesh", ".mesh_data");
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    AGUID GUID;
    GUID.Generate();

    GuidMap[GUID] = "/Root/" + fileName;

    bool bSkinnedMesh = m_bSkeletal;

    BvAxisAlignedBox BoundingBox;
    BoundingBox.Clear();
    for (MeshInfo const& meshInfo : m_Meshes)
    {
        BoundingBox.AddAABB(meshInfo.BoundingBox);
    }

    bool bRaycastBVH = m_Settings.bGenerateRaycastBVH && !bSkinnedMesh;

    f.WriteUInt32(FMT_FILE_TYPE_MESH);
    f.WriteUInt32(FMT_VERSION_MESH);
    f.WriteString(GUID.ToString());
    f.WriteBool(bSkinnedMesh);
    f.WriteObject(BoundingBox);
    f.WriteArray(m_Indices);
    f.WriteArray(m_Vertices);
    if (bSkinnedMesh)
    {
        f.WriteArray(m_Weights);
    }
    else
    {
        f.WriteUInt32(0); // weights count
    }
    f.WriteBool(bRaycastBVH); // only for static meshes
    f.WriteUInt16(m_Settings.RaycastPrimitivesPerLeaf);

    // Write subparts
    f.WriteUInt32(m_Meshes.Size()); // subparts count
    uint32_t n = 0;
    for (MeshInfo const& meshInfo : m_Meshes)
    {
        if (meshInfo.Mesh->name)
        {
            f.WriteString(meshInfo.Mesh->name);
        }
        else
        {
            f.WriteString(HK_FORMAT("Subpart_{}", n));
        }
        f.WriteInt32(meshInfo.BaseVertex);
        f.WriteUInt32(meshInfo.FirstIndex);
        f.WriteUInt32(meshInfo.VertexCount);
        f.WriteUInt32(meshInfo.IndexCount);
        f.WriteObject(meshInfo.BoundingBox);

        n++;
    }

    if (bRaycastBVH)
    {
        for (MeshInfo const& meshInfo : m_Meshes)
        {
            // Generate subpart BVH
            BvhTree aabbTree(TArrayView<SMeshVertex>(m_Vertices), {m_Indices.ToPtr() + meshInfo.FirstIndex, (size_t)meshInfo.IndexCount}, meshInfo.BaseVertex, m_Settings.RaycastPrimitivesPerLeaf);

            // Write subpart BVH
            f.WriteObject(aabbTree);
        }
    }

    f.WriteUInt32(0); // sockets count

    if (bSkinnedMesh)
    {
        f.WriteArray(m_Skin.JointIndices);
        f.WriteArray(m_Skin.OffsetMatrices);
    }

    //if ( m_Settings.bGenerateStaticCollisions ) {
    //TPodVector< ACollisionTriangleSoupData::SSubpart > subparts;

    //subparts.Resize( m_Meshes.Size() );
    //for ( int i = 0 ; i < subparts.Size() ; i++ ) {
    //    subparts[i].BaseVertex = m_Meshes[i].BaseVertex;
    //    subparts[i].VertexCount = m_Meshes[i].VertexCount;
    //    subparts[i].FirstIndex = m_Meshes[i].FirstIndex;
    //    subparts[i].IndexCount = m_Meshes[i].IndexCount;
    //}

    //ACollisionTriangleSoupData * tris = CreateInstanceOf< ACollisionTriangleSoupData >();

    //tris->Initialize( (float *)&m_Vertices.ToPtr()->Position, sizeof( m_Vertices[0] ), m_Vertices.Size(),
    //    m_Indices.ToPtr(), m_Indices.Size(), subparts.ToPtr(), subparts.Size(), BoundingBox );

    //ACollisionTriangleSoupBVHData * bvh = CreateInstanceOf< ACollisionTriangleSoupBVHData >();
    //bvh->TrisData = tris;
    //bvh->BuildBVH();
    //bvh->Write( f );
    //}


    fileName       = GeneratePhysicalPath("mesh", ".mesh");
    fileSystemPath = GEngine->GetRootPath() + fileName;

    f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    f.FormattedPrint("Mesh \"{}\"\n", GuidMap[GUID]);

    if (bSkinnedMesh)
    {
        f.FormattedPrint("Skeleton \"{}\"\n", GuidMap[m_SkeletonGUID]);
    }
    else
    {
        f.FormattedPrint("Skeleton \"{}\"\n", "/Default/Skeleton/Default");
    }
    f.FormattedPrint("Subparts [\n");
    for (MeshInfo const& meshInfo : m_Meshes)
    {
        f.FormattedPrint("\"{}\"\n", GuidMap[GetMaterialGUID(meshInfo.Material)]);
    }
    f.FormattedPrint("]\n");
}

void AAssetImporter::WriteMeshes()
{
    for (MeshInfo& meshInfo : m_Meshes)
    {
        WriteMesh(meshInfo);
    }
}

void AAssetImporter::WriteMesh(MeshInfo const& Mesh)
{
    AString     fileName       = GeneratePhysicalPath(Mesh.Mesh->name ? Mesh.Mesh->name : "mesh", ".mesh_data");
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    bool bSkinnedMesh = m_bSkeletal;
    HK_ASSERT(bSkinnedMesh == false);

    GuidMap[Mesh.GUID] = "/Root/" + fileName;

    bool bRaycastBVH = m_Settings.bGenerateRaycastBVH;

    f.WriteUInt32(FMT_FILE_TYPE_MESH);
    f.WriteUInt32(FMT_VERSION_MESH);
    f.WriteString(Mesh.GUID.ToString());
    f.WriteBool(bSkinnedMesh);
    f.WriteObject(Mesh.BoundingBox);

    f.WriteUInt32(Mesh.IndexCount);
    unsigned int* indices = m_Indices.ToPtr() + Mesh.FirstIndex;
    for (int i = 0; i < Mesh.IndexCount; i++)
    {
        f.WriteUInt32(*indices++);
    }

    f.WriteUInt32(Mesh.VertexCount);
    SMeshVertex* verts = m_Vertices.ToPtr() + Mesh.BaseVertex;
    for (int i = 0; i < Mesh.VertexCount; i++)
    {
        verts->Write(f);
        verts++;
    }

    if (bSkinnedMesh)
    {
        f.WriteUInt32(Mesh.VertexCount); // weights count

        SMeshVertexSkin* weights = m_Weights.ToPtr() + Mesh.BaseVertex;
        for (int i = 0; i < Mesh.VertexCount; i++)
        {
            weights->Write(f);
            weights++;
        }
    }
    else
    {
        f.WriteUInt32(0); // weights count
    }
    f.WriteBool(bRaycastBVH); // only for static meshes
    f.WriteUInt16(m_Settings.RaycastPrimitivesPerLeaf);
    f.WriteUInt32(1); // subparts count
    if (Mesh.Mesh->name)
    {
        f.WriteString(Mesh.Mesh->name);
    }
    else
    {
        f.WriteString("Subpart_1");
    }
    f.WriteInt32(0);  // base vertex
    f.WriteUInt32(0); // first index
    f.WriteUInt32(Mesh.VertexCount);
    f.WriteUInt32(Mesh.IndexCount);
    f.WriteObject(Mesh.BoundingBox);

    if (bRaycastBVH)
    {
        // Generate subpart BVH
        BvhTree aabbTree(TArrayView<SMeshVertex>(m_Vertices.ToPtr() + Mesh.BaseVertex, m_Vertices.Size() - Mesh.BaseVertex),
                         TArrayView<unsigned int>(m_Indices.ToPtr() + Mesh.FirstIndex, (size_t)Mesh.IndexCount),
                         0,
                         m_Settings.RaycastPrimitivesPerLeaf);

        // Write subpart BVH
        f.WriteObject(aabbTree);
    }

    f.WriteUInt32(0); // sockets count

    if (bSkinnedMesh)
    {
        f.WriteArray(m_Skin.JointIndices);
        f.WriteArray(m_Skin.OffsetMatrices);
    }

    fileName       = GeneratePhysicalPath("mesh", ".mesh");
    fileSystemPath = GEngine->GetRootPath() + fileName;

    f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    f.FormattedPrint("Mesh \"{}\"\n", GuidMap[Mesh.GUID]);

    if (bSkinnedMesh)
    {
        f.FormattedPrint("Skeleton \"{}\"\n", GuidMap[m_SkeletonGUID]);
    }
    else
    {
        f.FormattedPrint("Skeleton \"{}\"\n", "/Default/Skeleton/Default");
    }
    f.FormattedPrint("Subparts [\n");
    f.FormattedPrint("\"{}\"\n", GuidMap[GetMaterialGUID(Mesh.Material)]);
    f.FormattedPrint("]\n");
}

bool ImportEnvironmentMapForSkybox(ImageStorage const& Skybox, AStringView EnvmapFile)
{
    TRef<RenderCore::ITexture> SourceMap, IrradianceMap, ReflectionMap;

    if (!Skybox || Skybox.GetDesc().Type != TEXTURE_CUBE)
    {
        LOG("ImportEnvironmentMapForSkybox: invalid skybox\n");
        return false;
    }

    int width = Skybox.GetDesc().Width;

    RenderCore::STextureDesc textureDesc;
    textureDesc.SetResolution(RenderCore::STextureResolutionCubemap(width));
    textureDesc.SetFormat(Skybox.GetDesc().Format);
    textureDesc.SetMipLevels(1);
    textureDesc.SetBindFlags(RenderCore::BIND_SHADER_RESOURCE);

    if (Skybox.NumChannels() == 1)
    {
        // Apply texture swizzle for single channel textures
        textureDesc.Swizzle.R = RenderCore::TEXTURE_SWIZZLE_R;
        textureDesc.Swizzle.G = RenderCore::TEXTURE_SWIZZLE_R;
        textureDesc.Swizzle.B = RenderCore::TEXTURE_SWIZZLE_R;
        textureDesc.Swizzle.A = RenderCore::TEXTURE_SWIZZLE_R;
    }

    GEngine->GetRenderDevice()->CreateTexture(textureDesc, &SourceMap);

    RenderCore::STextureRect rect;
    rect.Offset.X        = 0;
    rect.Offset.Y        = 0;
    rect.Offset.MipLevel = 0;
    rect.Dimension.X     = width;
    rect.Dimension.Y     = width;
    rect.Dimension.Z     = 1;

    ImageSubresourceDesc subresDesc;
    subresDesc.MipmapIndex = 0;

    for (int faceNum = 0; faceNum < 6; faceNum++)
    {
        rect.Offset.Z = faceNum;

        subresDesc.SliceIndex = faceNum;

        ImageSubresource subresouce = Skybox.GetSubresource(subresDesc);
        
        SourceMap->WriteRect(rect, subresouce.GetSizeInBytes(), 1, subresouce.GetData());
    }

    GEngine->GetRenderBackend()->GenerateIrradianceMap(SourceMap, &IrradianceMap);
    GEngine->GetRenderBackend()->GenerateReflectionMap(SourceMap, &ReflectionMap);

    // Preform some validation
    HK_ASSERT(IrradianceMap->GetDesc().Resolution.Width == IrradianceMap->GetDesc().Resolution.Height);
    HK_ASSERT(ReflectionMap->GetDesc().Resolution.Width == ReflectionMap->GetDesc().Resolution.Height);
    HK_ASSERT(IrradianceMap->GetDesc().Format == TEXTURE_FORMAT_R11G11B10_FLOAT);
    HK_ASSERT(ReflectionMap->GetDesc().Format == TEXTURE_FORMAT_R11G11B10_FLOAT);

    AFile f = AFile::OpenWrite(EnvmapFile);
    if (!f)
    {
        LOG("Failed to write {}\n", EnvmapFile);
        return false;
    }

    f.WriteUInt32(FMT_FILE_TYPE_ENVMAP);
    f.WriteUInt32(FMT_VERSION_ENVMAP);
    f.WriteUInt32(IrradianceMap->GetWidth());
    f.WriteUInt32(ReflectionMap->GetWidth());

    // Choose max width for memory allocation
    int maxSize = Math::Max(IrradianceMap->GetWidth(), ReflectionMap->GetWidth());

    TVector<uint32_t> buffer(maxSize * maxSize * 6);

    uint32_t* data = buffer.ToPtr();

    int numPixels = IrradianceMap->GetWidth() * IrradianceMap->GetWidth() * 6;
    IrradianceMap->Read(0, numPixels * sizeof(uint32_t), 4, data);

    f.WriteWords<uint32_t>(data, numPixels);

    for (int mipLevel = 0; mipLevel < ReflectionMap->GetDesc().NumMipLevels; mipLevel++)
    {
        int mipWidth = ReflectionMap->GetWidth() >> mipLevel;
        HK_ASSERT(mipWidth > 0);

        numPixels = mipWidth * mipWidth * 6;

        ReflectionMap->Read(mipLevel, numPixels * sizeof(uint32_t), 4, data);

        f.WriteWords<uint32_t>(data, numPixels);
    }
    return true;
}

bool ImportEnvironmentMapForSkybox(SkyboxImportSettings const& ImportSettings, AStringView EnvmapFile)
{
    ImageStorage image = LoadSkyboxImages(ImportSettings);

    if (!image)
    {
        return false;
    }

    return ImportEnvironmentMapForSkybox(image, EnvmapFile);
}

ImageStorage GenerateAtmosphereSkybox(uint32_t Resolution, Float3 const& LightDir)
{
    TRef<RenderCore::ITexture> skybox;
    GEngine->GetRenderBackend()->GenerateSkybox(512, LightDir, &skybox);

    int width = skybox->GetWidth();

    RenderCore::STextureRect rect;
    rect.Offset.X        = 0;
    rect.Offset.Y        = 0;
    rect.Offset.MipLevel = 0;
    rect.Dimension.X     = width;
    rect.Dimension.Y     = width;
    rect.Dimension.Z     = 1;

    ImageStorageDesc desc;
    desc.Type       = TEXTURE_CUBE;
    desc.Width      = width;
    desc.Height     = width;
    desc.SliceCount = 6;
    desc.NumMipmaps = 1;
    //desc.Format     = TEXTURE_FORMAT_BC6H_UFLOAT;
    desc.Format     = skybox->GetDesc().Format;
    desc.Flags      = IMAGE_STORAGE_NO_ALPHA;

    ImageStorage storage(desc);

    //size_t   sz = width * width * 4 * sizeof(float);
    //HeapBlob blob(sz);

    for (int faceNum = 0; faceNum < 6; faceNum++)
    {
        ImageSubresourceDesc subresDesc;
        subresDesc.SliceIndex = faceNum;
        subresDesc.MipmapIndex = 0;

        ImageSubresource subresource = storage.GetSubresource(subresDesc);

        rect.Offset.Z = faceNum;

        skybox->ReadRect(rect, subresource.GetSizeInBytes(), 4, subresource.GetData());

        //skybox->ReadRect(rect, blob.Size(), 4, blob.GetData());

        //TextureBlockCompression::CompressBC6h(blob.GetData(), subresource.GetData(), width, width, false);

        //blob.ZeroMem();

        //DecompressBC6h(subresource.GetData(), blob.GetData(), width, width, false);

        //AFile f = AFile::OpenWrite(HK_FORMAT("Face{}.exr", faceNum));
        //WriteEXR(f, width, width, 3, (const float*)blob.GetData(), true);
    }
    return storage;
}

bool SaveSkyboxTexture(AStringView FileName, ImageStorage& Image)
{
    if (!Image || Image.GetDesc().Type != TEXTURE_CUBE)
    {
        LOG("SaveSkyboxTexture: invalid skybox\n");
        return false;
    }

    AFile f = AFile::OpenWrite(FileName);
    if (!f)
    {
        LOG("Failed to write {}\n", FileName);
        return false;
    }

    f.WriteUInt32(FMT_FILE_TYPE_TEXTURE);
    f.WriteUInt32(FMT_VERSION_TEXTURE);

    f.WriteObject(Image);
    
    f.WriteUInt32(6); // num source files
    for (int i = 0; i < 6; i++)
    {
        f.WriteString("Generated"); // source file
    }
    return true;
}

bool AAssetImporter::ImportSkybox(SAssetImportSettings const& ImportSettings)
{
    m_Settings            = ImportSettings;
    m_Settings.ImportFile = "Skybox";

    if (!ImportSettings.bImportSkyboxExplicit)
    {
        return false;
    }

    ImageStorage image = LoadSkyboxImages(ImportSettings.SkyboxImport);
    if (!image)
        return false;

    AString     fileName       = GeneratePhysicalPath("texture", ".texture");
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return false;
    }

    f.WriteUInt32(FMT_FILE_TYPE_TEXTURE);
    f.WriteUInt32(FMT_VERSION_TEXTURE);

    f.WriteObject(image);

    f.WriteUInt32(6); // num source files
    for (int i = 0; i < 6; i++)
    {
        f.WriteString(ImportSettings.SkyboxImport.Faces[i]); // source file
    }

    if (m_Settings.bCreateSkyboxMaterialInstance)
    {
        WriteSkyboxMaterial("/Root/" + fileName);
    }

    return true;
}

void AAssetImporter::WriteSkyboxMaterial(AStringView SkyboxTexture)
{
    AString     fileName       = GeneratePhysicalPath("matinst", ".minst");
    AString     fileSystemPath = GEngine->GetRootPath() + fileName;

    AFile f = AFile::OpenWrite(fileSystemPath);
    if (!f)
    {
        LOG("Failed to write {}\n", fileName);
        return;
    }

    f.FormattedPrint("Material \"/Default/Materials/Skybox\"\n");
    f.FormattedPrint("Textures [\n");
    f.FormattedPrint("\"{}\"\n", SkyboxTexture);
    f.FormattedPrint("]\n");
}





#include "lwo/lwo2.h"

#define MAX_MEMORY_LWO (16 << 10)

using ALinearAllocatorLWO = TLinearAllocator<MAX_MEMORY_LWO>;

static void* lwAlloc(void* _Allocator, size_t _Size)
{
    ALinearAllocatorLWO& Allocator = *static_cast<ALinearAllocatorLWO*>(_Allocator);
    void*                ptr       = Allocator.Allocate(_Size);
    Platform::ZeroMem(ptr, _Size);
    return ptr;
}

static void lwFree(void* _Allocator, void* _Bytes)
{
}

static size_t lwRead(void* _Buffer, size_t _ElementSize, size_t _ElementCount, struct st_lwFile* _Stream)
{
    IBinaryStreamReadInterface* stream = (IBinaryStreamReadInterface*)_Stream->UserData;

    size_t total = _ElementSize * _ElementCount;

    return stream->Read(_Buffer, total) / _ElementSize;
}

static int lwSeek(struct st_lwFile* _Stream, long _Offset, int _Origin)
{
    IBinaryStreamReadInterface* stream = (IBinaryStreamReadInterface*)_Stream->UserData;

    switch (_Origin)
    {
        case SEEK_CUR:
            return stream->SeekCur(_Offset) ? 0 : -1;
        case SEEK_SET:
            return stream->SeekSet(_Offset) ? 0 : -1;
        case SEEK_END:
            return stream->SeekEnd(_Offset) ? 0 : -1;
    }

    return -1;
}

static long lwTell(struct st_lwFile* _Stream)
{
    IBinaryStreamReadInterface* stream = (IBinaryStreamReadInterface*)_Stream->UserData;

    return (long)stream->GetOffset();
}

static int lwGetc(struct st_lwFile* _Stream)
{
    IBinaryStreamReadInterface* stream = (IBinaryStreamReadInterface*)_Stream->UserData;

    uint8_t c;
    if (stream->Read(&c, sizeof(c)) == 0)
    {
        _Stream->error = 1;
        return EOF;
    }

    return c;
}

struct SFace
{
    BvAxisAlignedBox Bounds;

    int FirstVertex;
    int NumVertices;

    int FirstIndex;
    int NumIndices;

    AMaterialInstance* MaterialInst;
};

static bool CreateIndexedMeshFromSurfaces(SFace const* InSurfaces, int InSurfaceCount, SMeshVertex const* InVertices, unsigned int const* InIndices, AIndexedMesh** IndexedMesh)
{
    int totalVerts    = 0;
    int totalIndices  = 0;
    int totalSubparts = 0;

    if (!InSurfaceCount)
    {
        return false;
    }

    TPodVector<SFace const*> surfaces;
    for (int j = 0; j < InSurfaceCount; j++)
    {
        surfaces.Add(&InSurfaces[j]);
    }

    struct SSortFunction
    {
        bool operator()(SFace const* _A, SFace const* _B)
        {
            return (_A->MaterialInst < _B->MaterialInst);
        }
    } SortFunction;

    auto CanMergeSurfaces = [](SFace const* InFirst, SFace const* InSecond) -> bool
    {
        return (InFirst->MaterialInst == InSecond->MaterialInst);
    };

    std::sort(surfaces.Begin(), surfaces.End(), SortFunction);

    SFace const* merge = surfaces[0];
    totalSubparts      = 1;
    for (int j = 0; j < surfaces.Size(); j++)
    {
        SFace const* surf = surfaces[j];

        totalVerts += surf->NumVertices;
        totalIndices += surf->NumIndices;

        if (!CanMergeSurfaces(surf, merge))
        {
            totalSubparts++;
            merge = surf;
        }
    }

    //Float3x3 normalMatrix;
    //InPretransform.DecomposeNormalMatrix( normalMatrix );

    AIndexedMesh* indexedMesh = AIndexedMesh::Create(totalVerts, totalIndices, totalSubparts, false);

    SMeshVertex*  verts   = indexedMesh->GetVertices();
    unsigned int* indices = indexedMesh->GetIndices();

    int              baseVertex         = 0;
    int              firstIndex         = 0;
    int              subpartVertexCount = 0;
    int              subpartIndexCount  = 0;
    BvAxisAlignedBox subpartBounds;

    merge            = surfaces[0];
    int subpartIndex = 0;
    subpartBounds.Clear();
    for (int j = 0; j < surfaces.Size(); j++)
    {
        SFace const* surf = surfaces[j];

        if (!CanMergeSurfaces(surf, merge))
        {

            AIndexedMeshSubpart* subpart = indexedMesh->GetSubpart(subpartIndex);
            subpart->SetBaseVertex(baseVertex);
            subpart->SetFirstIndex(firstIndex);
            subpart->SetVertexCount(subpartVertexCount);
            subpart->SetIndexCount(subpartIndexCount);
            subpart->SetMaterialInstance(merge->MaterialInst);
            subpart->SetBoundingBox(subpartBounds);

            CalcTangentSpace(indexedMesh->GetVertices() + baseVertex, subpartVertexCount, indexedMesh->GetIndices() + firstIndex, subpartIndexCount);

            // Begin new subpart
            firstIndex += subpartIndexCount;
            baseVertex += subpartVertexCount;
            subpartIndexCount  = 0;
            subpartVertexCount = 0;
            merge              = surfaces[j];
            subpartIndex++;
            subpartBounds.Clear();
        }

        for (int v = 0; v < surf->NumVertices; v++, verts++)
        {
            *verts = InVertices[surf->FirstVertex + v];
            subpartBounds.AddPoint(verts->Position);
        }

        for (int v = 0; v < surf->NumIndices;)
        {
            *indices++ = subpartVertexCount + InIndices[surf->FirstIndex + v++]; // - surf->FirstVertex;
            *indices++ = subpartVertexCount + InIndices[surf->FirstIndex + v++]; // - surf->FirstVertex;
            *indices++ = subpartVertexCount + InIndices[surf->FirstIndex + v++]; // - surf->FirstVertex;
        }

        subpartVertexCount += surf->NumVertices;
        subpartIndexCount += surf->NumIndices;
    }

    AIndexedMeshSubpart* subpart = indexedMesh->GetSubpart(subpartIndex);
    subpart->SetBaseVertex(baseVertex);
    subpart->SetFirstIndex(firstIndex);
    subpart->SetVertexCount(subpartVertexCount);
    subpart->SetIndexCount(subpartIndexCount);
    subpart->SetMaterialInstance(merge->MaterialInst);
    subpart->SetBoundingBox(subpartBounds);

    CalcTangentSpace(indexedMesh->GetVertices() + baseVertex, subpartVertexCount, indexedMesh->GetIndices() + firstIndex, subpartIndexCount);

    indexedMesh->SendVertexDataToGPU(totalVerts, 0);
    indexedMesh->SendIndexDataToGPU(totalIndices, 0);

    *IndexedMesh = indexedMesh;

    return true;
}

static bool CreateLWOMesh(lwObject* lwo, float InScale, AMaterialInstance* (*GetMaterial)(const char* _Name), AIndexedMesh** IndexedMesh)
{
#define USE_COLOR

    lwSurface* lwoSurf;
    int        j, k;
    Float3     normal;

#ifdef USE_COLOR
    byte color[4];
#endif

    if (!lwo->surf)
    {
        return false;
    }

    if (!lwo->layer)
    {
        return false;
    }

    lwLayer* layer = lwo->layer;

    if (layer->point.count <= 0)
    {
        // no vertex data
        return false;
    }

    TPodVector<Float3> verts;

    verts.Resize(layer->point.count);
    for (j = 0; j < layer->point.count; j++)
    {
        verts[j].X = layer->point.pt[j].pos[0];
        verts[j].Y = layer->point.pt[j].pos[1];
        verts[j].Z = -layer->point.pt[j].pos[2];
    }

    int numUVs = 0;
    if (layer->nvmaps)
    {
        for (lwVMap* vm = layer->vmap; vm; vm = vm->next)
        {
            if (vm->type == LWID_('T', 'X', 'U', 'V'))
            {
                numUVs += vm->nverts;
            }
        }
    }

    TPodVector<Float2> texCoors;
    texCoors.Resize(numUVs);
    int offset = 0;
    for (lwVMap* vm = layer->vmap; vm; vm = vm->next)
    {
        if (vm->type == LWID_('T', 'X', 'U', 'V'))
        {
            vm->offset = offset;
            for (k = 0; k < vm->nverts; k++)
            {
                texCoors[k + offset].X = vm->val[k][0];
                texCoors[k + offset].Y = 1.0f - vm->val[k][1];
            }
            offset += vm->nverts;
        }
    }
    if (!numUVs)
    {
        texCoors.Resize(1);
        texCoors[0] = Float2(0.0f);
        numUVs      = 1;
    }

    TPodVector<int> vertexMap;
    TPodVector<int> texcoordMap;

    vertexMap.Resize(layer->point.count);
    for (j = 0; j < layer->point.count; j++)
    {
        vertexMap[j] = j;
    }

    texcoordMap.Resize(numUVs);
    for (j = 0; j < numUVs; j++)
    {
        texcoordMap[j] = j;
    }

    struct SMatchVert
    {
        int    v;
        int    uv;
        Float3 normal;
#ifdef USE_COLOR
        byte color[4];
#endif
        SMatchVert* next;
    };

    TPodVector<SMatchVert*> matchHash;
    TPodVector<SMatchVert>  tempVertices;
    SMatchVert *            lastmv, *mv;

    TPodVector<SFace>        faces;
    TPodVector<SMeshVertex>  modelVertices;
    TPodVector<unsigned int> modelIndices;

    int numFaces = 0;
    for (lwoSurf = lwo->surf; lwoSurf; lwoSurf = lwoSurf->next)
    {
        if (layer->polygon.count > 0)
        {
            numFaces++;
        }
    }

    faces.Resize(numFaces);

    int faceIndex = 0;

    for (lwoSurf = lwo->surf; lwoSurf; lwoSurf = lwoSurf->next)
    {
        bool bMatchNormals = true;

        if (layer->polygon.count <= 0)
        {
            continue;
        }

        SFace& face = faces[faceIndex++];

        int firstVert   = modelVertices.Size();
        int firstIndex  = modelIndices.Size();
        int numVertices = 0;
        int numIndices  = 0;

        tempVertices.ResizeInvalidate(layer->polygon.count * 3);
        tempVertices.ZeroMem();

        modelIndices.Resize(firstIndex + layer->polygon.count * 3);

        unsigned int* pindices = modelIndices.ToPtr() + firstIndex;

        matchHash.ResizeInvalidate(layer->point.count);
        matchHash.ZeroMem();

        for (j = 0; j < layer->polygon.count; j++)
        {
            lwPolygon* poly = &layer->polygon.pol[j];

            if (poly->surf != lwoSurf)
            {
                continue;
            }

            if (poly->nverts != 3)
            {
                LOG("CreateLWOMesh: polygon has {} verts, expected triangle\n", poly->nverts);
                continue;
            }

            for (k = 0; k < 3; k++)
            {

                int v    = vertexMap[poly->v[k].index];
                normal.X = poly->v[k].norm[0];
                normal.Y = poly->v[k].norm[1];
                normal.Z = -poly->v[k].norm[2];
                normal.NormalizeFix();

                int uv = 0;

#ifdef USE_COLOR
                color[0] = lwoSurf->color.rgb[0] * 255;
                color[1] = lwoSurf->color.rgb[1] * 255;
                color[2] = lwoSurf->color.rgb[2] * 255;
                color[3] = 255;
#endif

                // Set attributes from the vertex
                lwPoint* pt = &layer->point.pt[poly->v[k].index];
                for (int nvm = 0; nvm < pt->nvmaps; nvm++)
                {
                    lwVMapPt* vm = &pt->vm[nvm];

                    if (vm->vmap->type == LWID_('T', 'X', 'U', 'V'))
                    {
                        uv = texcoordMap[vm->index + vm->vmap->offset];
                    }
#ifdef USE_COLOR
                    if (vm->vmap->type == LWID_('R', 'G', 'B', 'A'))
                    {
                        for (int chan = 0; chan < 4; chan++)
                        {
                            color[chan] = 255 * vm->vmap->val[vm->index][chan];
                        }
                    }
#endif
                }

                // Override with polygon attributes
                for (int nvm = 0; nvm < poly->v[k].nvmaps; nvm++)
                {
                    lwVMapPt* vm = &poly->v[k].vm[nvm];

                    if (vm->vmap->type == LWID_('T', 'X', 'U', 'V'))
                    {
                        uv = texcoordMap[vm->index + vm->vmap->offset];
                    }
#ifdef USE_COLOR
                    if (vm->vmap->type == LWID_('R', 'G', 'B', 'A'))
                    {
                        for (int chan = 0; chan < 4; chan++)
                        {
                            color[chan] = 255 * vm->vmap->val[vm->index][chan];
                        }
                    }
#endif
                }

                // find a matching vert
                for (lastmv = NULL, mv = matchHash[v]; mv != NULL; lastmv = mv, mv = mv->next)
                {
                    if (mv->uv != uv)
                    {
                        continue;
                    }
#ifdef USE_COLOR
                    if (*(unsigned*)mv->color != *(unsigned*)color)
                    {
                        continue;
                    }
#endif
                    if (!bMatchNormals || mv->normal.CompareEps(normal, 0.0001f))
                    {
                        //if ( mv->normal * normal > normalEpsilon ) {
                        break;
                    }
                }
                if (!mv)
                {
                    // allocate a new match vert and link to hash chain
                    mv         = &tempVertices[numVertices++];
                    mv->v      = v;
                    mv->uv     = uv;
                    mv->normal = normal;
#ifdef USE_COLOR
                    *(unsigned*)mv->color = *(unsigned*)color;
#endif
                    mv->next = NULL;
                    if (lastmv)
                    {
                        lastmv->next = mv;
                    }
                    else
                    {
                        matchHash[v] = mv;
                    }
                }

                pindices[numIndices++] = mv - tempVertices.ToPtr(); //pindices;
            }
        }

        for (j = 0; j < numIndices; j += 3)
        {
            std::swap(pindices[j], pindices[j + 2]);
        }

        modelVertices.Resize(firstVert + numVertices);

        face.Bounds.Clear();

        // Copy vertices
        SMeshVertex* pvert = modelVertices.ToPtr() + firstVert;
        for (j = 0; j < numVertices; j++, pvert++)
        {
            mv              = &tempVertices[j];
            pvert->Position = verts[mv->v];
            pvert->SetTexCoord(texCoors[mv->uv]);
            pvert->SetNormal(mv->normal);
            pvert->Position *= InScale;
            //*(unsigned *)pvert->color = *(unsigned *)mv->color;
            face.Bounds.AddPoint(pvert->Position);
        }

        face.FirstVertex  = firstVert;
        face.FirstIndex   = firstIndex;
        face.NumVertices  = numVertices;
        face.NumIndices   = numIndices;
        face.MaterialInst = GetMaterial(lwoSurf->name);
    }

    return CreateIndexedMeshFromSurfaces(faces.ToPtr(), faces.Size(), modelVertices.ToPtr(), modelIndices.ToPtr(), IndexedMesh);
}

bool LoadLWO(IBinaryStreamReadInterface& InStream, float InScale, AMaterialInstance* (*GetMaterial)(const char* _Name), AIndexedMesh** IndexedMesh)
{
    lwFile              file;
    unsigned int        failID;
    int                 failPos;
    ALinearAllocatorLWO Allocator;

    file.Read      = lwRead;
    file.Seek      = lwSeek;
    file.Tell      = lwTell;
    file.Getc      = lwGetc;
    file.Alloc     = lwAlloc;
    file.Free      = lwFree;
    file.UserData  = &InStream;
    file.Allocator = &Allocator;
    file.error     = 0;

    lwObject* lwo = lwGetObject(&file, &failID, &failPos);
    if (!lwo)
    {
        return false;
    }

    bool ret = CreateLWOMesh(lwo, InScale, GetMaterial, IndexedMesh);

    // We don't call lwFreeObject becouse our linear allocator frees it automatically.

    return ret;
}
