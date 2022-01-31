﻿/*

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

#include "CollisionModel.h"
#include "IndexedMesh.h"
#include "ConvexDecomposition.h"

#include <Platform/Logger.h>

#include "BulletCompatibility.h"

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btMultiSphereShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btConeShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btStaticPlaneShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btConvexPointCloudShape.h>
#include <BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btStridingMeshInterface.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>

#ifdef AN_COMPILER_MSVC
#    pragma warning(push)
#    pragma warning(disable : 4456 4828)
#endif
#include <BulletCollision/Gimpact/btGImpactShape.h>
#ifdef AN_COMPILER_MSVC
#    pragma warning(pop)
#endif

#include <BulletCollision/CollisionShapes/btMultimaterialTriangleMeshShape.h>

#ifdef BULLET_WORLD_IMPORTER
#    include <bullet3/Extras/Serialize/BulletWorldImporter/btBulletWorldImporter.h>
#endif

AN_CLASS_META(ACollisionModel)

ATTRIBUTE_ALIGNED16(class)
AStridingMeshInterface : public btStridingMeshInterface
{
public:
    Float3*                Vertices;
    unsigned int*          Indices;
    SCollisionMeshSubpart* Subparts;
    int                    SubpartCount;
    mutable btVector3      AABBMin;
    mutable btVector3      AABBMax;
    mutable bool           bHasAABB{};

    void getLockedVertexIndexBase(unsigned char** VertexBase,
                                  int&            VertexCount,
                                  PHY_ScalarType& Type,
                                  int&            VertexStride,
                                  unsigned char** IndexBase,
                                  int&            IndexStride,
                                  int&            FaceCount,
                                  PHY_ScalarType& IndexType,
                                  int             Subpart = 0) override
    {

        AN_ASSERT(Subpart < SubpartCount);

        auto* subpart = Subparts + Subpart;

        (*VertexBase) = (unsigned char*)(Vertices + subpart->BaseVertex);
        VertexCount   = subpart->VertexCount;
        Type          = PHY_FLOAT;
        VertexStride  = sizeof(Vertices[0]);

        (*IndexBase) = (unsigned char*)(Indices + subpart->FirstIndex);
        IndexStride  = sizeof(Indices[0]) * 3;
        FaceCount    = subpart->IndexCount / 3;
        IndexType    = PHY_INTEGER;
    }

    void getLockedReadOnlyVertexIndexBase(const unsigned char** VertexBase,
                                          int&                  VertexCount,
                                          PHY_ScalarType&       Type,
                                          int&                  VertexStride,
                                          const unsigned char** IndexBase,
                                          int&                  IndexStride,
                                          int&                  FaceCount,
                                          PHY_ScalarType&       IndexType,
                                          int                   Subpart = 0) const override
    {
        AN_ASSERT(Subpart < SubpartCount);

        auto* subpart = Subparts + Subpart;

        (*VertexBase) = (const unsigned char*)(Vertices + subpart->BaseVertex);
        VertexCount   = subpart->VertexCount;
        Type          = PHY_FLOAT;
        VertexStride  = sizeof(Vertices[0]);

        (*IndexBase) = (const unsigned char*)(Indices + subpart->FirstIndex);
        IndexStride  = sizeof(Indices[0]) * 3;
        FaceCount    = subpart->IndexCount / 3;
        IndexType    = PHY_INTEGER;
    }

    // unLockVertexBase finishes the access to a subpart of the triangle mesh
    // make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
    void unLockVertexBase(int subpart) override { (void)subpart; }

    void unLockReadOnlyVertexBase(int subpart) const override { (void)subpart; }

    // getNumSubParts returns the number of seperate subparts
    // each subpart has a continuous array of vertices and indices
    int getNumSubParts() const override
    {
        return SubpartCount;
    }

    void preallocateVertices(int numverts) override { (void)numverts; }
    void preallocateIndices(int numindices) override { (void)numindices; }

    bool hasPremadeAabb() const override { return bHasAABB; }

    void setPremadeAabb(const btVector3& aabbMin, const btVector3& aabbMax) const override
    {
        AABBMin  = aabbMin;
        AABBMax  = aabbMax;
        bHasAABB = true;
    }

    void getPremadeAabb(btVector3* aabbMin, btVector3* aabbMax) const override
    {
        *aabbMin = AABBMin;
        *aabbMax = AABBMax;
    }
};

struct ACollisionSphere : ACollisionBody
{
    float Radius           = 0.5f;
    bool  bNonUniformScale = false;

    btCollisionShape* Create() override
    {
        if (bNonUniformScale)
        {
            btVector3 pos(0, 0, 0);
            return new btMultiSphereShape(&pos, &Radius, 1);
        }
        else
        {
            return new btSphereShape(Radius);
        }
    }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        float sinTheta, cosTheta, sinPhi, cosPhi;

        const float detail = Math::Floor(Math::Max(1.0f, Radius) + 0.5f);

        const int numStacks = 8 * detail;
        const int numSlices = 12 * detail;

        const int vertexCount = (numStacks + 1) * numSlices;
        const int indexCount  = numStacks * numSlices * 6;

        const int firstVertex = _Vertices.Size();
        const int firstIndex  = _Indices.Size();

        _Vertices.Resize(firstVertex + vertexCount);
        _Indices.Resize(firstIndex + indexCount);

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        Float3x4 transform = Transform;
        if (!bNonUniformScale)
        {
            float sqrScaleX = Float3(transform[0][0], transform[1][0], transform[2][0]).LengthSqr();
            float sqrScaleY = Float3(transform[0][1], transform[1][1], transform[2][1]).LengthSqr();
            float sqrScaleZ = Float3(transform[0][2], transform[1][2], transform[2][2]).LengthSqr();

            float sy = std::sqrt(sqrScaleX / sqrScaleY);
            float sz = std::sqrt(sqrScaleX / sqrScaleZ);

            transform[0][1] *= sy;
            transform[0][2] *= sz;
            transform[1][1] *= sy;
            transform[1][2] *= sz;
            transform[2][1] *= sy;
            transform[2][2] *= sz;
        }

        for (int stack = 0; stack <= numStacks; ++stack)
        {
            const float theta = stack * Math::_PI / numStacks;
            Math::SinCos(theta, sinTheta, cosTheta);

            for (int slice = 0; slice < numSlices; ++slice)
            {
                const float phi = slice * Math::_2PI / numSlices;
                Math::SinCos(phi, sinPhi, cosPhi);

                *pVertices++ = transform * (Float3(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta) * Radius + Position);
            }
        }

        for (int stack = 0; stack < numStacks; ++stack)
        {
            const int stackOffset     = firstVertex + stack * numSlices;
            const int nextStackOffset = firstVertex + (stack + 1) * numSlices;

            for (int slice = 0; slice < numSlices; ++slice)
            {
                const int nextSlice = (slice + 1) % numSlices;
                *pIndices++         = stackOffset + slice;
                *pIndices++         = stackOffset + nextSlice;
                *pIndices++         = nextStackOffset + nextSlice;
                *pIndices++         = nextStackOffset + nextSlice;
                *pIndices++         = nextStackOffset + slice;
                *pIndices++         = stackOffset + slice;
            }
        }
    }
};

struct ACollisionSphereRadii : ACollisionBody
{
    Float3 Radius = Float3(0.5f);

    btCollisionShape* Create() override
    {
        btVector3           pos(0, 0, 0);
        float               radius = 1.0f;
        btMultiSphereShape* shape  = new btMultiSphereShape(&pos, &radius, 1);
        shape->setLocalScaling(btVectorToFloat3(Radius));
        return shape;
    }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        float sinTheta, cosTheta, sinPhi, cosPhi;

        const float detail = Math::Floor(Math::Max(1.0f, Radius.Max()) + 0.5f);

        const int numStacks = 8 * detail;
        const int numSlices = 12 * detail;

        const int vertexCount = (numStacks + 1) * numSlices;
        const int indexCount  = numStacks * numSlices * 6;

        const int firstVertex = _Vertices.Size();
        const int firstIndex  = _Indices.Size();

        _Vertices.Resize(firstVertex + vertexCount);
        _Indices.Resize(firstIndex + indexCount);

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        for (int stack = 0; stack <= numStacks; ++stack)
        {
            const float theta = stack * Math::_PI / numStacks;
            Math::SinCos(theta, sinTheta, cosTheta);

            for (int slice = 0; slice < numSlices; ++slice)
            {
                const float phi = slice * Math::_2PI / numSlices;
                Math::SinCos(phi, sinPhi, cosPhi);

                *pVertices++ = Transform * (Rotation * (Float3(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta) * Radius) + Position);
            }
        }

        for (int stack = 0; stack < numStacks; ++stack)
        {
            const int stackOffset     = firstVertex + stack * numSlices;
            const int nextStackOffset = firstVertex + (stack + 1) * numSlices;

            for (int slice = 0; slice < numSlices; ++slice)
            {
                const int nextSlice = (slice + 1) % numSlices;
                *pIndices++         = stackOffset + slice;
                *pIndices++         = stackOffset + nextSlice;
                *pIndices++         = nextStackOffset + nextSlice;
                *pIndices++         = nextStackOffset + nextSlice;
                *pIndices++         = nextStackOffset + slice;
                *pIndices++         = stackOffset + slice;
            }
        }
    }
};

struct ACollisionBox : ACollisionBody
{
    Float3 HalfExtents = Float3(0.5f);

    btCollisionShape* Create() override
    {
        return new btBoxShape(btVectorToFloat3(HalfExtents));
    }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        unsigned int const indices[36] = {0, 3, 2, 2, 1, 0, 7, 4, 5, 5, 6, 7, 3, 7, 6, 6, 2, 3, 2, 6, 5, 5, 1, 2, 1, 5, 4, 4, 0, 1, 0, 4, 7, 7, 3, 0};

        const int firstVertex = _Vertices.Size();
        const int firstIndex  = _Indices.Size();

        _Vertices.Resize(firstVertex + 8);
        _Indices.Resize(firstIndex + 36);

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        *pVertices++ = Transform * (Rotation * Float3(-HalfExtents.X, HalfExtents.Y, -HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(HalfExtents.X, HalfExtents.Y, -HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(HalfExtents.X, HalfExtents.Y, HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(-HalfExtents.X, HalfExtents.Y, HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(-HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(HalfExtents.X, -HalfExtents.Y, HalfExtents.Z) + Position);
        *pVertices++ = Transform * (Rotation * Float3(-HalfExtents.X, -HalfExtents.Y, HalfExtents.Z) + Position);

        for (int i = 0; i < 36; i++)
        {
            *pIndices++ = firstVertex + indices[i];
        }
    }
};

struct ACollisionCylinder : ACollisionBody
{
    Float3 HalfExtents = Float3(1.0f);
    int    Axial       = COLLISION_SHAPE_AXIAL_DEFAULT;

    btCollisionShape* Create() override
    {
        switch (Axial)
        {
            case COLLISION_SHAPE_AXIAL_X:
                return new btCylinderShapeX(btVectorToFloat3(HalfExtents));
            case COLLISION_SHAPE_AXIAL_Y:
                return new btCylinderShape(btVectorToFloat3(HalfExtents));
            case COLLISION_SHAPE_AXIAL_Z:
                return new btCylinderShapeZ(btVectorToFloat3(HalfExtents));
        }
        return new btCylinderShape(btVectorToFloat3(HalfExtents));
    }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        float sinPhi, cosPhi;

        int idxRadius, idxRadius2, idxHeight;
        switch (Axial)
        {
            case COLLISION_SHAPE_AXIAL_X:
                idxRadius  = 1;
                idxRadius2 = 2;
                idxHeight  = 0;
                break;
            case COLLISION_SHAPE_AXIAL_Z:
                idxRadius  = 0;
                idxRadius2 = 1;
                idxHeight  = 2;
                break;
            case COLLISION_SHAPE_AXIAL_Y:
            default:
                idxRadius  = 0;
                idxRadius2 = 2;
                idxHeight  = 1;
                break;
        }

        const float detal = Math::Floor(Math::Max(1.0f, HalfExtents[idxRadius]) + 0.5f);

        const int numSlices     = 8 * detal;
        const int faceTriangles = numSlices - 2;

        const int vertexCount = numSlices * 2;
        const int indexCount  = faceTriangles * 3 * 2 + numSlices * 6;

        const int firstIndex  = _Indices.Size();
        const int firstVertex = _Vertices.Size();

        _Vertices.Resize(firstVertex + vertexCount);
        _Indices.Resize(firstIndex + indexCount);

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        Float3 vert;

        for (int slice = 0; slice < numSlices; slice++, pVertices++)
        {
            Math::SinCos(slice * Math::_2PI / numSlices, sinPhi, cosPhi);

            vert[idxRadius]  = cosPhi * HalfExtents[idxRadius];
            vert[idxRadius2] = sinPhi * HalfExtents[idxRadius];
            vert[idxHeight]  = HalfExtents[idxHeight];

            *pVertices = Transform * (Rotation * vert + Position);

            vert[idxHeight] = -vert[idxHeight];

            *(pVertices + numSlices) = Transform * (Rotation * vert + Position);
        }

        const int offset     = firstVertex;
        const int nextOffset = firstVertex + numSlices;

        // top face
        for (int i = 0; i < faceTriangles; i++)
        {
            *pIndices++ = offset + i + 2;
            *pIndices++ = offset + i + 1;
            *pIndices++ = offset + 0;
        }

        // bottom face
        for (int i = 0; i < faceTriangles; i++)
        {
            *pIndices++ = nextOffset + i + 1;
            *pIndices++ = nextOffset + i + 2;
            *pIndices++ = nextOffset + 0;
        }

        for (int slice = 0; slice < numSlices; ++slice)
        {
            const int nextSlice = (slice + 1) % numSlices;
            *pIndices++         = offset + slice;
            *pIndices++         = offset + nextSlice;
            *pIndices++         = nextOffset + nextSlice;
            *pIndices++         = nextOffset + nextSlice;
            *pIndices++         = nextOffset + slice;
            *pIndices++         = offset + slice;
        }
    }
};

struct ACollisionCone : ACollisionBody
{
    float Radius = 1;
    float Height = 1;
    int   Axial  = COLLISION_SHAPE_AXIAL_DEFAULT;

    btCollisionShape* Create() override
    {
        switch (Axial)
        {
            case COLLISION_SHAPE_AXIAL_X:
                return new btConeShapeX(Radius, Height);
            case COLLISION_SHAPE_AXIAL_Y:
                return new btConeShape(Radius, Height);
            case COLLISION_SHAPE_AXIAL_Z:
                return new btConeShapeZ(Radius, Height);
        }
        return new btConeShape(Radius, Height);
    }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        float sinPhi, cosPhi;

        int idxRadius, idxRadius2, idxHeight;
        switch (Axial)
        {
            case COLLISION_SHAPE_AXIAL_X:
                idxRadius  = 1;
                idxRadius2 = 2;
                idxHeight  = 0;
                break;
            case COLLISION_SHAPE_AXIAL_Z:
                idxRadius  = 0;
                idxRadius2 = 1;
                idxHeight  = 2;
                break;
            case COLLISION_SHAPE_AXIAL_Y:
            default:
                idxRadius  = 0;
                idxRadius2 = 2;
                idxHeight  = 1;
                break;
        }

        const float detal = Math::Floor(Math::Max(1.0f, Radius) + 0.5f);

        const int numSlices     = 8 * detal;
        const int faceTriangles = numSlices - 2;

        const int vertexCount = numSlices + 1;
        const int indexCount  = faceTriangles * 3 + numSlices * 3;

        const int firstIndex  = _Indices.Size();
        const int firstVertex = _Vertices.Size();

        _Vertices.Resize(firstVertex + vertexCount);
        _Indices.Resize(firstIndex + indexCount);

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        Float3 vert;

        vert.Clear();
        vert[idxHeight] = Height;

        // top point
        *pVertices++ = Transform * (Rotation * vert + Position);

        vert[idxHeight] = 0;

        for (int slice = 0; slice < numSlices; slice++)
        {
            Math::SinCos(slice * Math::_2PI / numSlices, sinPhi, cosPhi);

            vert[idxRadius]  = cosPhi * Radius;
            vert[idxRadius2] = sinPhi * Radius;

            *pVertices++ = Transform * (Rotation * vert + Position);
        }

        const int offset = firstVertex + 1;

        // bottom face
        for (int i = 0; i < faceTriangles; i++)
        {
            *pIndices++ = offset + 0;
            *pIndices++ = offset + i + 1;
            *pIndices++ = offset + i + 2;
        }

        // sides
        for (int slice = 0; slice < numSlices; ++slice)
        {
            *pIndices++ = firstVertex;
            *pIndices++ = offset + (slice + 1) % numSlices;
            *pIndices++ = offset + slice;
        }
    }
};

struct ACollisionCapsule : ACollisionBody
{
    /** Radius of the capsule. The total height is Height + 2 * Radius */
    float Radius = 1;

    /** Height between the center of each sphere of the capsule caps */
    float Height = 1;

    int Axial = COLLISION_SHAPE_AXIAL_DEFAULT;

    btCollisionShape* Create() override
    {
        switch (Axial)
        {
            case COLLISION_SHAPE_AXIAL_X:
                return new btCapsuleShapeX(Radius, Height);
            case COLLISION_SHAPE_AXIAL_Y:
                return new btCapsuleShape(Radius, Height);
            case COLLISION_SHAPE_AXIAL_Z:
                return new btCapsuleShapeZ(Radius, Height);
        }
        return new btCapsuleShape(Radius, Height);
    }

    float GetTotalHeight() const { return Height + 2 * Radius; }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        int idxRadius, idxRadius2, idxHeight;
        switch (Axial)
        {
            case COLLISION_SHAPE_AXIAL_X:
                idxRadius  = 1;
                idxRadius2 = 2;
                idxHeight  = 0;
                break;
            case COLLISION_SHAPE_AXIAL_Z:
                idxRadius  = 0;
                idxRadius2 = 1;
                idxHeight  = 2;
                break;
            case COLLISION_SHAPE_AXIAL_Y:
            default:
                idxRadius  = 0;
                idxRadius2 = 2;
                idxHeight  = 1;
                break;
        }

#if 0
        // Capsule is implemented as cylinder

        float sinPhi, cosPhi;

        const float detal = Math::Floor( Math::Max( 1.0f, Radius ) + 0.5f );

        const int numSlices = 8 * detal;
        const int faceTriangles = numSlices - 2;

        const int vertexCount = numSlices * 2;
        const int indexCount = faceTriangles * 3 * 2 + numSlices * 6;

        const int firstIndex = _Indices.Size();
        const int firstVertex = _Vertices.Size();

        _Vertices.Resize( firstVertex + vertexCount );
        _Indices.Resize( firstIndex + indexCount );

        Float3 * pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int * pIndices = _Indices.ToPtr() + firstIndex;

        Float3 vert;

        const float halfOfTotalHeight = Height*0.5f + Radius;

        for ( int slice = 0; slice < numSlices; slice++, pVertices++ ) {
            Math::SinCos( slice * Math::_2PI / numSlices, sinPhi, cosPhi );

            vert[idxRadius] = cosPhi * Radius;
            vert[idxRadius2] = sinPhi * Radius;
            vert[idxHeight] = halfOfTotalHeight;

            *pVertices = Rotation * vert + Position;

            vert[idxHeight] = -vert[idxHeight];

            *(pVertices + numSlices ) = Rotation * vert + Position;
        }

        const int offset = firstVertex;
        const int nextOffset = firstVertex + numSlices;

        // top face
        for ( int i = 0 ; i < faceTriangles; i++ ) {
            *pIndices++ = offset + i + 2;
            *pIndices++ = offset + i + 1;
            *pIndices++ = offset + 0;
        }

        // bottom face
        for ( int i = 0 ; i < faceTriangles; i++ ) {
            *pIndices++ = nextOffset + i + 1;
            *pIndices++ = nextOffset + i + 2;
            *pIndices++ = nextOffset + 0;
        }

        for ( int slice = 0; slice < numSlices; ++slice ) {
            int nextSlice = (slice + 1) % numSlices;
            *pIndices++ = offset + slice;
            *pIndices++ = offset + nextSlice;
            *pIndices++ = nextOffset + nextSlice;
            *pIndices++ = nextOffset + nextSlice;
            *pIndices++ = nextOffset + slice;
            *pIndices++ = offset + slice;
        }
#else
        int   x, y;
        float verticalAngle, horizontalAngle;

        unsigned int quad[4];

        const float detail = Math::Floor(Math::Max(1.0f, Radius) + 0.5f);

        const int numVerticalSubdivs   = 6 * detail;
        const int numHorizontalSubdivs = 8 * detail;

        const int halfVerticalSubdivs = numVerticalSubdivs >> 1;

        const int vertexCount = (numHorizontalSubdivs + 1) * (numVerticalSubdivs + 1) * 2;
        const int indexCount  = numHorizontalSubdivs * (numVerticalSubdivs + 1) * 6;

        const int firstIndex  = _Indices.Size();
        const int firstVertex = _Vertices.Size();

        _Vertices.Resize(firstVertex + vertexCount);
        _Indices.Resize(firstIndex + indexCount);

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        const float verticalStep   = Math::_PI / numVerticalSubdivs;
        const float horizontalStep = Math::_2PI / numHorizontalSubdivs;

        const float halfHeight = Height * 0.5f;

        for (y = 0, verticalAngle = -Math::_HALF_PI; y <= halfVerticalSubdivs; y++)
        {
            float h, r;
            Math::SinCos(verticalAngle, h, r);
            h = h * Radius - halfHeight;
            r *= Radius;
            for (x = 0, horizontalAngle = 0; x <= numHorizontalSubdivs; x++)
            {
                float   s, c;
                Float3& v = *pVertices++;
                Math::SinCos(horizontalAngle, s, c);
                v[idxRadius]  = r * c;
                v[idxRadius2] = r * s;
                v[idxHeight]  = h;
                v             = Transform * (Rotation * v + Position);
                horizontalAngle += horizontalStep;
            }
            verticalAngle += verticalStep;
        }

        for (y = 0, verticalAngle = 0; y <= halfVerticalSubdivs; y++)
        {
            float h, r;
            Math::SinCos(verticalAngle, h, r);
            h = h * Radius + halfHeight;
            r *= Radius;
            for (x = 0, horizontalAngle = 0; x <= numHorizontalSubdivs; x++)
            {
                float   s, c;
                Float3& v = *pVertices++;
                Math::SinCos(horizontalAngle, s, c);
                v[idxRadius]  = r * c;
                v[idxRadius2] = r * s;
                v[idxHeight]  = h;
                v             = Transform * (Rotation * v + Position);
                horizontalAngle += horizontalStep;
            }
            verticalAngle += verticalStep;
        }

        for (y = 0; y <= numVerticalSubdivs; y++)
        {
            const int y2 = y + 1;
            for (x = 0; x < numHorizontalSubdivs; x++)
            {
                const int x2 = x + 1;
                quad[0]      = firstVertex + y * (numHorizontalSubdivs + 1) + x;
                quad[1]      = firstVertex + y2 * (numHorizontalSubdivs + 1) + x;
                quad[2]      = firstVertex + y2 * (numHorizontalSubdivs + 1) + x2;
                quad[3]      = firstVertex + y * (numHorizontalSubdivs + 1) + x2;
                *pIndices++  = quad[0];
                *pIndices++  = quad[1];
                *pIndices++  = quad[2];
                *pIndices++  = quad[2];
                *pIndices++  = quad[3];
                *pIndices++  = quad[0];
            }
        }
#endif
    }
};

struct ACollisionConvexHull : ACollisionBody
{
    TPodVectorHeap<Float3>       Vertices;
    TPodVectorHeap<unsigned int> Indices;

    btCollisionShape* Create() override
    {
        #if 0
            constexpr bool bComputeAabb = false; // FIXME: Do we need to calc aabb now?
            // NOTE: btConvexPointCloudShape keeps pointer to vertices
            return new btConvexPointCloudShape( &Vertices[0][0], Vertices.Size(), btVector3(1.f,1.f,1.f), bComputeAabb );
        #else
            // NOTE: btConvexHullShape keeps copy of vertices
            return new btConvexHullShape(&Vertices[0][0], Vertices.Size(), sizeof(Float3));
        #endif
    }

    bool IsConvex() const override { return true; }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        if (_Vertices.IsEmpty())
        {
            return;
        }

        const int firstVertex = _Vertices.Size();
        const int firstIndex  = _Indices.Size();

        _Vertices.Resize(firstVertex + Vertices.Size());
        _Indices.Resize(firstIndex + Indices.Size());

        Float3*       pVertices = _Vertices.ToPtr() + firstVertex;
        unsigned int* pIndices  = _Indices.ToPtr() + firstIndex;

        for (int i = 0; i < Vertices.Size(); i++)
        {
            *pVertices++ = Transform * (Rotation * Vertices[i] + Position);
        }

        for (int i = 0; i < Indices.Size(); i++)
        {
            *pIndices++ = firstVertex + Indices[i];
        }
    }
};


// ACollisionTriangleSoupBVH can be used only for static or kinematic objects
struct ACollisionTriangleSoupBVH : ACollisionBody
{
    TPodVectorHeap<Float3>                Vertices;
    TPodVectorHeap<unsigned int>          Indices;
    TPodVectorHeap<SCollisionMeshSubpart> Subparts;
    BvAxisAlignedBox                      BoundingBox;
    TUniqueRef<AStridingMeshInterface>    pInterface;

    btCollisionShape* Create() override
    {
        return new btScaledBvhTriangleMeshShape(Data.GetObject(), btVector3(1.f, 1.f, 1.f));

        // TODO: Create GImpact mesh shape for dynamic objects
    }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        if (Vertices.IsEmpty())
        {
            return;
        }

        const int firstIndex  = _Indices.Size();
        const int firstVertex = _Vertices.Size();

        _Vertices.Resize(firstVertex + Vertices.Size());

        int indexCount = 0;
        for (auto& subpart : Subparts)
        {
            indexCount += subpart.IndexCount;
        }

        _Indices.Resize(firstIndex + indexCount);

        unsigned int* pIndices = _Indices.ToPtr() + firstIndex;

        for (auto& subpart : Subparts)
        {
            for (int i = 0; i < subpart.IndexCount; i++)
            {
                *pIndices++ = firstVertex + subpart.BaseVertex + Indices[subpart.FirstIndex + i];
            }
        }

        Float3* pVertices = _Vertices.ToPtr() + firstVertex;

        for (int i = 0; i < Vertices.Size(); i++)
        {
            *pVertices++ = Transform * (Rotation * Vertices[i] + Position);
        }
    }

    void BuildBVH(bool bForceQuantizedAabbCompression = false)
    {
        pInterface->Vertices     = Vertices.ToPtr();
        pInterface->Indices      = Indices.ToPtr();
        pInterface->Subparts     = Subparts.ToPtr();
        pInterface->SubpartCount = Subparts.Size();

        if (!bForceQuantizedAabbCompression)
        {
            constexpr unsigned int QUANTIZED_AABB_COMPRESSION_MAX_TRIANGLES = 1000000;

            int indexCount = 0;
            for (int i = 0; i < pInterface->SubpartCount; i++)
            {
                indexCount += pInterface->Subparts[i].IndexCount;
            }

            // NOTE: With too many triangles, Bullet will not work correctly with Quantized Aabb Compression
            bUsedQuantizedAabbCompression = indexCount / 3 <= QUANTIZED_AABB_COMPRESSION_MAX_TRIANGLES;
        }
        else
        {
            bUsedQuantizedAabbCompression = true;
        }

        Data = MakeUnique<btBvhTriangleMeshShape>(pInterface.GetObject(),
                                                  bUsedQuantizedAabbCompression,
                                                  btVectorToFloat3(BoundingBox.Mins),
                                                  btVectorToFloat3(BoundingBox.Maxs),
                                                  true);

        TriangleInfoMap = MakeUnique<btTriangleInfoMap>();
        btGenerateInternalEdgeInfo(Data.GetObject(), TriangleInfoMap.GetObject());
    }

    bool UsedQuantizedAabbCompression() const
    {
        return bUsedQuantizedAabbCompression;
    }

#ifdef BULLET_WORLD_IMPORTER
    void Read( IBinaryStream & _Stream ) {
        uint32_t bufferSize;
        _Stream >> bufferSize;
        byte * buffer = (byte *)GHeapMemory.Alloc( bufferSize );
        _Stream.Read( buffer, bufferSize );

        btBulletWorldImporter Importer(0);
        if ( Importer.loadFileFromMemory( (char *)buffer, bufferSize ) ) {
            Data = (btBvhTriangleMeshShape*)Importer.getCollisionShapeByIndex( 0 );
        }

        GHeapMemory.HeapFree( buffer );
    }

    void Write( IBinaryStream & _Stream ) const {
        if ( Data ) {
            btDefaultSerializer Serializer;

            Serializer.startSerialization();

            Data->serializeSingleBvh( &Serializer );
            Data->serializeSingleTriangleInfoMap( &Serializer );

            Serializer.finishSerialization();

            _Stream << uint32_t( Serializer.getCurrentBufferSize() );
            _Stream.Write( Serializer.getBufferPointer(), Serializer.getCurrentBufferSize() );
        }
    }
#endif

private:
    TUniqueRef<btBvhTriangleMeshShape> Data; // TODO: Try btMultimaterialTriangleMeshShape
    TUniqueRef<btTriangleInfoMap>      TriangleInfoMap;

    bool bUsedQuantizedAabbCompression = false;
};

struct ACollisionTriangleSoupGimpact : ACollisionBody
{
    TPodVectorHeap<Float3>                Vertices;
    TPodVectorHeap<unsigned int>          Indices;
    TPodVectorHeap<SCollisionMeshSubpart> Subparts;
    BvAxisAlignedBox                      BoundingBox;
    TUniqueRef<AStridingMeshInterface>    pInterface;

    btCollisionShape* Create() override
    {
        // FIXME: This shape don't work. Why?
        pInterface->Vertices     = Vertices.ToPtr();
        pInterface->Indices      = Indices.ToPtr();
        pInterface->Subparts     = Subparts.ToPtr();
        pInterface->SubpartCount = Subparts.Size();
        return new btGImpactMeshShape(pInterface.GetObject());
    }

    void GatherGeometry(TPodVectorHeap<Float3>& _Vertices, TPodVectorHeap<unsigned int>& _Indices, Float3x4 const& Transform) const override
    {
        if (Vertices.IsEmpty())
        {
            return;
        }

        const int firstIndex  = _Indices.Size();
        const int firstVertex = _Vertices.Size();

        _Vertices.Resize(firstVertex + Vertices.Size());

        int indexCount = 0;
        for (auto& subpart : Subparts)
        {
            indexCount += subpart.IndexCount;
        }

        _Indices.Resize(firstIndex + indexCount);

        unsigned int* pIndices = _Indices.ToPtr() + firstIndex;

        for (auto& subpart : Subparts)
        {
            for (int i = 0; i < subpart.IndexCount; i++)
            {
                *pIndices++ = firstVertex + subpart.BaseVertex + Indices[subpart.FirstIndex + i];
            }
        }

        Float3* pVertices = _Vertices.ToPtr() + firstVertex;

        for (int i = 0; i < Vertices.Size(); i++)
        {
            *pVertices++ = Transform * (Rotation * Vertices[i] + Position);
        }
    }
};

ACollisionModel::ACollisionModel()
{}

ACollisionModel::~ACollisionModel()
{}

void ACollisionModel::Initialize(void const* pShapes)
{
    Purge();

    int numShapes = 0;

    while (pShapes)
    {
        COLLISION_SHAPE type = *(COLLISION_SHAPE const*)pShapes;
        switch (type)
        {
            case COLLISION_SHAPE_SPHERE:
                AddSphere((SCollisionSphereDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_SPHERE_RADII:
                AddSphereRadii((SCollisionSphereRadiiDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_BOX:
                AddBox((SCollisionBoxDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_CYLINDER:
                AddCylinder((SCollisionCylinderDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_CONE:
                AddCone((SCollisionConeDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_CAPSULE:
                AddCapsule((SCollisionCapsuleDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_CONVEX_HULL:
                AddConvexHull((SCollisionConvexHullDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_TRIANGLE_SOUP_BVH:
                AddTriangleSoupBVH((SCollisionTriangleSoupBVHDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_TRIANGLE_SOUP_GIMPACT:
                AddTriangleSoupGimpact((SCollisionTriangleSoupGimpactDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_CONVEX_DECOMPOSITION:
                AddConvexDecomposition((SCollisionConvexDecompositionDef const*)pShapes, numShapes);
                break;
            case COLLISION_SHAPE_CONVEX_DECOMPOSITION_VHACD:
                AddConvexDecompositionVHACD((SCollisionConvexDecompositionVHACDDef const*)pShapes, numShapes);
                break;
            default:
                GLogger.Printf("ACollisionModel::Initialize: unknown shape type\n");
                pShapes = nullptr;
                continue;
        }
        pShapes = ((SCollisionSphereDef const*)pShapes)->pNext;
    }

    if (numShapes)
    {
        CenterOfMass /= numShapes;
    }
}

void ACollisionModel::Initialize(SCollisionModelCreateInfo const& CreateInfo)
{
    Initialize(CreateInfo.pShapes);

    if (CreateInfo.bOverrideCenterOfMass)
    {
        CenterOfMass = CreateInfo.CenterOfMass;
    }
}

void ACollisionModel::Purge()
{
    CollisionBodies.Clear();
    CenterOfMass.Clear();
    BoneCollisions.Clear();
}

void ACollisionModel::AddSphere(SCollisionSphereDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionSphere>();

    body->Position         = pShape->Position;
    body->Margin           = pShape->Margin;
    body->Radius           = pShape->Radius;
    body->bNonUniformScale = pShape->bNonUniformScale;

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddSphereRadii(SCollisionSphereRadiiDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionSphereRadii>();

    body->Position = pShape->Position;
    body->Rotation = pShape->Rotation;
    body->Margin   = pShape->Margin;
    body->Radius   = pShape->Radius;

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddBox(SCollisionBoxDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionBox>();

    body->Position    = pShape->Position;
    body->Rotation    = pShape->Rotation;
    body->Margin      = pShape->Margin;
    body->HalfExtents = pShape->HalfExtents;

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddCylinder(SCollisionCylinderDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionCylinder>();

    body->Position    = pShape->Position;
    body->Rotation    = pShape->Rotation;
    body->Margin      = pShape->Margin;
    body->HalfExtents = pShape->HalfExtents;
    body->Axial       = pShape->Axial;

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddCone(SCollisionConeDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionCone>();

    body->Position = pShape->Position;
    body->Rotation = pShape->Rotation;
    body->Margin   = pShape->Margin;
    body->Radius   = pShape->Radius;
    body->Height   = pShape->Height;
    body->Axial    = pShape->Axial;

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddCapsule(SCollisionCapsuleDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionCapsule>();

    body->Position = pShape->Position;
    body->Rotation = pShape->Rotation;
    body->Margin   = pShape->Margin;
    body->Radius   = pShape->Radius;
    body->Height   = pShape->Height;
    body->Axial    = pShape->Axial;

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddConvexHull(SCollisionConvexHullDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionConvexHull>();

    body->Position = pShape->Position;
    body->Rotation = pShape->Rotation;
    body->Margin   = pShape->Margin;

    if (pShape->pVertices && pShape->pIndices && pShape->VertexCount && pShape->IndexCount)
    {
        body->Vertices.ResizeInvalidate(pShape->VertexCount);
        body->Indices.ResizeInvalidate(pShape->IndexCount);

        Platform::Memcpy(body->Vertices.ToPtr(), pShape->pVertices, pShape->VertexCount * sizeof(Float3));
        Platform::Memcpy(body->Indices.ToPtr(), pShape->pIndices, pShape->IndexCount * sizeof(unsigned int));
    }
    else if (pShape->pPlanes && pShape->PlaneCount)
    {
        for (int i = 0; i < pShape->PlaneCount; i++)
        {
            AConvexHull* hull = AConvexHull::CreateForPlane(pShape->pPlanes[i]);

            for (int j = 0; j < pShape->PlaneCount; j++)
            {
                if (i != j)
                {
                    AConvexHull* front;
                    hull->Clip(-pShape->pPlanes[j], 0.001f, &front);
                    hull->Destroy();
                    hull = front;
                }
            }

            if (!hull || hull->NumPoints < 3)
            {
                if (hull)
                    hull->Destroy();
                GLogger.Printf("ACollisionModel::AddConvexHull: hull is clipped off\n");
                return;
            }

            int firstIndex = body->Indices.Size();
            for (int v = 0; v < hull->NumPoints; v++)
            {
                int hasVert = body->Vertices.Size();
                for (int t = 0; t < body->Vertices.Size(); t++)
                {
                    Float3& vert = body->Vertices[t];
                    if ((vert - hull->Points[v]).LengthSqr() > FLT_EPSILON)
                    {
                        continue;
                    }
                    hasVert = t;
                    break;
                }
                if (hasVert == body->Vertices.Size())
                {
                    body->Vertices.Append(hull->Points[v]);
                }
                if (v > 2)
                {
                    body->Indices.Append(body->Indices[firstIndex]);
                    body->Indices.Append(body->Indices[body->Indices.Size() - 2]);
                }
                body->Indices.Append(hasVert);
            }

            hull->Destroy();
        }
    }
    else
    {
        GLogger.Printf("ACollisionModel::AddConvexHull: undefined geometry\n");
        return;
    }

    if (pShape->Bone.JointIndex >= 0)
    {
        SBoneCollision boneCol;
        boneCol.JointIndex     = pShape->Bone.JointIndex;
        boneCol.CollisionGroup = pShape->Bone.CollisionGroup;
        boneCol.CollisionMask  = pShape->Bone.CollisionMask;
        boneCol.CollisionBody  = std::move(body);
        BoneCollisions.emplace_back(std::move(boneCol));
    }
    else
    {
        CenterOfMass += body->Position;
        NumShapes++;
        CollisionBodies.emplace_back(std::move(body));
    }
}

void ACollisionModel::AddTriangleSoupBVH(SCollisionTriangleSoupBVHDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionTriangleSoupBVH>();

    body->pInterface = MakeUnique<AStridingMeshInterface>();

    body->Position = pShape->Position;
    body->Rotation = pShape->Rotation;
    body->Margin   = pShape->Margin;

    body->Vertices.ResizeInvalidate(pShape->VertexCount);
    body->Indices.ResizeInvalidate(pShape->IndexCount);
    body->Subparts.ResizeInvalidate(pShape->SubpartCount);

    if (pShape->VertexStride == sizeof(body->Vertices[0]))
    {
        Platform::Memcpy(body->Vertices.ToPtr(), pShape->pVertices, sizeof(body->Vertices[0]) * pShape->VertexCount);
    }
    else
    {
        byte const* ptr = (byte const*)pShape->pVertices;
        for (int i = 0; i < pShape->VertexCount; i++)
        {
            Platform::Memcpy(body->Vertices.ToPtr() + i, ptr, sizeof(body->Vertices[0]));
            ptr += pShape->VertexStride;
        }
    }

    Platform::Memcpy(body->Indices.ToPtr(), pShape->pIndices, sizeof(body->Indices[0]) * pShape->IndexCount);

    if (pShape->pSubparts)
    {
        body->BoundingBox.Clear();
        body->Subparts.Resize(pShape->SubpartCount);
        for (int i = 0; i < pShape->SubpartCount; i++)
        {
            SCollisionMeshSubpart const& subpart = pShape->pSubparts[i];

            body->Subparts[i].BaseVertex  = subpart.BaseVertex;
            body->Subparts[i].VertexCount = subpart.VertexCount;
            body->Subparts[i].FirstIndex  = subpart.FirstIndex;
            body->Subparts[i].IndexCount  = subpart.IndexCount;

            Float3 const* pVertices = pShape->pVertices + subpart.BaseVertex;
            unsigned int const* pIndices = pShape->pIndices + subpart.FirstIndex;
            for (int n = 0; n < subpart.IndexCount; n += 3)
            {
                unsigned int i0 = pIndices[n];
                unsigned int i1 = pIndices[n + 1];
                unsigned int i2 = pIndices[n + 2];

                body->BoundingBox.AddPoint(pVertices[i0]);
                body->BoundingBox.AddPoint(pVertices[i1]);
                body->BoundingBox.AddPoint(pVertices[i2]);
            }
        }
    }
    else if (pShape->pIndexedMeshSubparts)
    {
        body->BoundingBox.Clear();
        body->Subparts.Resize(pShape->SubpartCount);
        for (int i = 0; i < pShape->SubpartCount; i++)
        {
            body->Subparts[i].BaseVertex  = pShape->pIndexedMeshSubparts[i]->GetBaseVertex();
            body->Subparts[i].VertexCount = pShape->pIndexedMeshSubparts[i]->GetVertexCount();
            body->Subparts[i].FirstIndex  = pShape->pIndexedMeshSubparts[i]->GetFirstIndex();
            body->Subparts[i].IndexCount  = pShape->pIndexedMeshSubparts[i]->GetIndexCount();
            body->BoundingBox.AddAABB(pShape->pIndexedMeshSubparts[i]->GetBoundingBox());
        }
    }
    else
    {
        body->BoundingBox.Clear();

        body->Subparts.Resize(1);
        body->Subparts[0].BaseVertex  = 0;
        body->Subparts[0].VertexCount = pShape->VertexCount;
        body->Subparts[0].FirstIndex  = 0;
        body->Subparts[0].IndexCount  = pShape->IndexCount;

        Float3 const*       pVertices = pShape->pVertices;
        unsigned int const* pIndices  = pShape->pIndices;
        for (int n = 0; n < pShape->IndexCount; n += 3)
        {
            unsigned int i0 = pIndices[n];
            unsigned int i1 = pIndices[n + 1];
            unsigned int i2 = pIndices[n + 2];

            body->BoundingBox.AddPoint(pVertices[i0]);
            body->BoundingBox.AddPoint(pVertices[i1]);
            body->BoundingBox.AddPoint(pVertices[i2]);
        }
    }

    body->BuildBVH(pShape->bForceQuantizedAabbCompression);

    CenterOfMass += body->Position;
    NumShapes++;

    CollisionBodies.emplace_back(std::move(body));
}

void ACollisionModel::AddTriangleSoupGimpact(SCollisionTriangleSoupGimpactDef const* pShape, int& NumShapes)
{
    auto body = MakeUnique<ACollisionTriangleSoupGimpact>();

    body->pInterface = MakeUnique<AStridingMeshInterface>();

    body->Position = pShape->Position;
    body->Rotation = pShape->Rotation;
    body->Margin   = pShape->Margin;

    body->Vertices.ResizeInvalidate(pShape->VertexCount);
    body->Indices.ResizeInvalidate(pShape->IndexCount);
    body->Subparts.ResizeInvalidate(pShape->SubpartCount);

    if (pShape->VertexStride == sizeof(body->Vertices[0]))
    {
        Platform::Memcpy(body->Vertices.ToPtr(), pShape->pVertices, sizeof(body->Vertices[0]) * pShape->VertexCount);
    }
    else
    {
        byte const* ptr = (byte const*)pShape->pVertices;
        for (int i = 0; i < pShape->VertexCount; i++)
        {
            Platform::Memcpy(body->Vertices.ToPtr() + i, ptr, sizeof(body->Vertices[0]));
            ptr += pShape->VertexStride;
        }
    }

    Platform::Memcpy(body->Indices.ToPtr(), pShape->pIndices, sizeof(body->Indices[0]) * pShape->IndexCount);

    if (pShape->pSubparts)
    {
        body->BoundingBox.Clear();
        body->Subparts.Resize(pShape->SubpartCount);
        for (int i = 0; i < pShape->SubpartCount; i++)
        {
            SCollisionMeshSubpart const& subpart = pShape->pSubparts[i];

            body->Subparts[i].BaseVertex  = subpart.BaseVertex;
            body->Subparts[i].VertexCount = subpart.VertexCount;
            body->Subparts[i].FirstIndex  = subpart.FirstIndex;
            body->Subparts[i].IndexCount  = subpart.IndexCount;

            Float3 const*       pVertices = pShape->pVertices + subpart.BaseVertex;
            unsigned int const* pIndices  = pShape->pIndices + subpart.FirstIndex;
            for (int n = 0; n < subpart.IndexCount; n += 3)
            {
                unsigned int i0 = pIndices[n];
                unsigned int i1 = pIndices[n + 1];
                unsigned int i2 = pIndices[n + 2];

                body->BoundingBox.AddPoint(pVertices[i0]);
                body->BoundingBox.AddPoint(pVertices[i1]);
                body->BoundingBox.AddPoint(pVertices[i2]);
            }
        }
    }
    else if (pShape->pIndexedMeshSubparts)
    {
        body->BoundingBox.Clear();
        body->Subparts.Resize(pShape->SubpartCount);
        for (int i = 0; i < pShape->SubpartCount; i++)
        {
            body->Subparts[i].BaseVertex  = pShape->pIndexedMeshSubparts[i]->GetBaseVertex();
            body->Subparts[i].VertexCount = pShape->pIndexedMeshSubparts[i]->GetVertexCount();
            body->Subparts[i].FirstIndex  = pShape->pIndexedMeshSubparts[i]->GetFirstIndex();
            body->Subparts[i].IndexCount  = pShape->pIndexedMeshSubparts[i]->GetIndexCount();
            body->BoundingBox.AddAABB(pShape->pIndexedMeshSubparts[i]->GetBoundingBox());
        }
    }
    else
    {
        body->BoundingBox.Clear();

        body->Subparts.Resize(1);
        body->Subparts[0].BaseVertex  = 0;
        body->Subparts[0].VertexCount = pShape->VertexCount;
        body->Subparts[0].FirstIndex  = 0;
        body->Subparts[0].IndexCount  = pShape->IndexCount;

        Float3 const*       pVertices = pShape->pVertices;
        unsigned int const* pIndices  = pShape->pIndices;
        for (int n = 0; n < pShape->IndexCount; n += 3)
        {
            unsigned int i0 = pIndices[n];
            unsigned int i1 = pIndices[n + 1];
            unsigned int i2 = pIndices[n + 2];

            body->BoundingBox.AddPoint(pVertices[i0]);
            body->BoundingBox.AddPoint(pVertices[i1]);
            body->BoundingBox.AddPoint(pVertices[i2]);
        }
    }

    CenterOfMass += body->Position;
    NumShapes++;

    CollisionBodies.emplace_back(std::move(body));
}

void ACollisionModel::AddConvexDecomposition(SCollisionConvexDecompositionDef const* pShape, int& NumShapes)
{
    TPodVector<Float3>          hullVertices;
    TPodVector<unsigned int>    hullIndices;
    TPodVector<SConvexHullDesc> hulls;

    ::PerformConvexDecomposition(pShape->pVertices,
                                 pShape->VerticesCount,
                                 pShape->VertexStride,
                                 pShape->pIndices,
                                 pShape->IndicesCount,
                                 hullVertices,
                                 hullIndices,
                                 hulls);

    if (hulls.IsEmpty())
    {
        return;
    }

    Float3 saveCenterOfMass = CenterOfMass;

    CenterOfMass.Clear();

    int n{};
    for (SConvexHullDesc const& hull : hulls)
    {
        SCollisionConvexHullDef hulldef;

        hulldef.Position    = hull.Centroid;
        hulldef.Margin      = 0.01f;
        hulldef.pVertices   = hullVertices.ToPtr() + hull.FirstVertex;
        hulldef.VertexCount = hull.VertexCount;
        hulldef.pIndices    = hullIndices.ToPtr() + hull.FirstIndex;
        hulldef.IndexCount  = hull.IndexCount;

        AddConvexHull(&hulldef, n);
    }

    CenterOfMass /= n;
    CenterOfMass += saveCenterOfMass;
    NumShapes++;
}

void ACollisionModel::AddConvexDecompositionVHACD(SCollisionConvexDecompositionVHACDDef const* pShape, int& NumShapes)
{
    TPodVector<Float3>          hullVertices;
    TPodVector<unsigned int>    hullIndices;
    TPodVector<SConvexHullDesc> hulls;
    Float3                      decompositionCenterOfMass;

    ::PerformConvexDecompositionVHACD(pShape->pVertices,
                                      pShape->VerticesCount,
                                      pShape->VertexStride,
                                      pShape->pIndices,
                                      pShape->IndicesCount,
                                      hullVertices,
                                      hullIndices,
                                      hulls,
                                      decompositionCenterOfMass);

    if (hulls.IsEmpty())
    {
        return;
    }

    CenterOfMass += decompositionCenterOfMass;
    NumShapes++;

    // Save current center of mass
    Float3 saveCenterOfMass = CenterOfMass;

    int n{};
    for (SConvexHullDesc const& hull : hulls)
    {
        SCollisionConvexHullDef hulldef;

        hulldef.Position    = hull.Centroid;
        hulldef.Margin      = 0.01f;
        hulldef.pVertices   = hullVertices.ToPtr() + hull.FirstVertex;
        hulldef.VertexCount = hull.VertexCount;
        hulldef.pIndices    = hullIndices.ToPtr() + hull.FirstIndex;
        hulldef.IndexCount  = hull.IndexCount;

        AddConvexHull(&hulldef, n);
    }

    // Restore center of mass to ignore computations in AddConvexHull
    CenterOfMass = saveCenterOfMass;
}

void ACollisionModel::GatherGeometry(TPodVectorHeap<Float3>& Vertices, TPodVectorHeap<unsigned int>& Indices, Float3x4 const& Transform) const
{
    for (TUniqueRef<ACollisionBody> const& collisionBody : CollisionBodies)
    {
        collisionBody->GatherGeometry(Vertices, Indices, Transform);
    }
}

TRef<ACollisionInstance> ACollisionModel::Instantiate(Float3 const& Scale)
{
    return MakeRef<ACollisionInstance>(this, Scale);
}

ACollisionInstance::ACollisionInstance(ACollisionModel* CollisionModel, Float3 const& Scale)
{
    constexpr float POSITION_COMPARE_EPSILON {0.0001f};

    Model = CollisionModel;
    CompoundShape = MakeUnique<btCompoundShape>();
    CenterOfMass  = Scale * CollisionModel->GetCenterOfMass();

    if (!CollisionModel->GetCollisionBodies().IsEmpty())
    {
        const btVector3 scaling = btVectorToFloat3(Scale);
        btTransform     shapeTransform;

        for (TUniqueRef<ACollisionBody> const& collisionBody : CollisionModel->GetCollisionBodies())
        {
            btCollisionShape* shape = collisionBody->Create();

            shape->setMargin(collisionBody->Margin);
            shape->setLocalScaling(shape->getLocalScaling() * scaling);

            shapeTransform.setOrigin(btVectorToFloat3(Scale * collisionBody->Position - CenterOfMass));
            shapeTransform.setRotation(btQuaternionToQuat(collisionBody->Rotation));

            CompoundShape->addChildShape(shapeTransform, shape);
        }
    }

    int  numShapes    = CompoundShape->getNumChildShapes();
    bool bUseCompound = !numShapes || numShapes > 1;
    if (!bUseCompound)
    {
        btTransform const& childTransform = CompoundShape->getChildTransform(0);

        if (!btVectorToFloat3(childTransform.getOrigin()).CompareEps(Float3::Zero(), POSITION_COMPARE_EPSILON) || btQuaternionToQuat(childTransform.getRotation()) != Quat::Identity())
        {
            bUseCompound = true;
        }
    }

    CollisionShape = bUseCompound ? CompoundShape.GetObject() : CompoundShape->getChildShape(0);
}

ACollisionInstance::~ACollisionInstance()
{
    int numShapes = CompoundShape->getNumChildShapes();
    for (int i = numShapes - 1; i >= 0; i--)
    {
        btCollisionShape* shape = CompoundShape->getChildShape(i);
        delete shape;
    }
}

Float3 ACollisionInstance::CalculateLocalInertia(float Mass) const
{
    btVector3 localInertia;
    CollisionShape->calculateLocalInertia(Mass, localInertia);
    return btVectorToFloat3(localInertia);
}

void ACollisionInstance::GetCollisionBodiesWorldBounds(Float3 const& WorldPosition, Quat const& WorldRotation, TPodVector<BvAxisAlignedBox>& BoundingBoxes) const
{
    btVector3 mins, maxs;

    btTransform transform;
    transform.setOrigin(btVectorToFloat3(WorldPosition));
    transform.setRotation(btQuaternionToQuat(WorldRotation));

    int numShapes = CompoundShape->getNumChildShapes();
    BoundingBoxes.ResizeInvalidate(numShapes);

    for (int i = 0; i < numShapes; i++)
    {
        btCompoundShapeChild& shape = CompoundShape->getChildList()[i];

        shape.m_childShape->getAabb(transform * shape.m_transform, mins, maxs);

        BoundingBoxes[i].Mins = btVectorToFloat3(mins);
        BoundingBoxes[i].Maxs = btVectorToFloat3(maxs);
    }
}

void ACollisionInstance::GetCollisionWorldBounds(Float3 const& WorldPosition, Quat const& WorldRotation, BvAxisAlignedBox& BoundingBox) const
{
    btVector3 mins, maxs;

    btTransform transform;
    transform.setOrigin(btVectorToFloat3(WorldPosition));
    transform.setRotation(btQuaternionToQuat(WorldRotation));

    BoundingBox.Clear();

    int numShapes = CompoundShape->getNumChildShapes();

    for (int i = 0; i < numShapes; i++)
    {
        btCompoundShapeChild& shape = CompoundShape->getChildList()[i];

        shape.m_childShape->getAabb(transform * shape.m_transform, mins, maxs);

        BoundingBox.AddAABB(btVectorToFloat3(mins), btVectorToFloat3(maxs));
    }
}

void ACollisionInstance::GetCollisionBodyWorldBounds(int Index, Float3 const& WorldPosition, Quat const& WorldRotation, BvAxisAlignedBox& BoundingBox) const
{
    if (Index < 0 || Index >= CompoundShape->getNumChildShapes())
    {
        GLogger.Printf("ACollisionInstance::GetCollisionBodyWorldBounds: invalid index\n");

        BoundingBox.Clear();
        return;
    }

    btVector3 mins, maxs;

    btTransform transform;
    transform.setOrigin(btVectorToFloat3(WorldPosition));
    transform.setRotation(btQuaternionToQuat(WorldRotation));

    btCompoundShapeChild& shape = CompoundShape->getChildList()[Index];

    shape.m_childShape->getAabb(transform * shape.m_transform, mins, maxs);

    BoundingBox.Mins = btVectorToFloat3(mins);
    BoundingBox.Maxs = btVectorToFloat3(maxs);
}

void ACollisionInstance::GetCollisionBodyLocalBounds(int Index, BvAxisAlignedBox& BoundingBox) const
{
    if (Index < 0 || Index >= CompoundShape->getNumChildShapes())
    {
        GLogger.Printf("ACollisionInstance::GetCollisionBodyLocalBounds: invalid index\n");

        BoundingBox.Clear();
        return;
    }

    btVector3 mins, maxs;

    btCompoundShapeChild& shape = CompoundShape->getChildList()[Index];

    shape.m_childShape->getAabb(shape.m_transform, mins, maxs);

    BoundingBox.Mins = btVectorToFloat3(mins);
    BoundingBox.Maxs = btVectorToFloat3(maxs);
}

float ACollisionInstance::GetCollisionBodyMargin(int Index) const
{
    if (Index < 0 || Index >= CompoundShape->getNumChildShapes())
    {
        GLogger.Printf("ACollisionInstance::GetCollisionBodyMargin: invalid index\n");

        return 0;
    }

    btCompoundShapeChild& shape = CompoundShape->getChildList()[Index];

    return shape.m_childShape->getMargin();
}

int ACollisionInstance::GetCollisionBodiesCount() const
{
    return CompoundShape->getNumChildShapes();
}
