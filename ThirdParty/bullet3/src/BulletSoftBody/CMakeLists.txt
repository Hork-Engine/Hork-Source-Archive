
INCLUDE_DIRECTORIES(
${BULLET_PHYSICS_SOURCE_DIR}/src

)

#SUBDIRS( Solvers )

SET(BulletSoftBody_SRCS
	btSoftBody.cpp
	btSoftBodyConcaveCollisionAlgorithm.cpp
	btSoftBodyHelpers.cpp
	btSoftBodyRigidBodyCollisionConfiguration.cpp
	btSoftRigidCollisionAlgorithm.cpp
	btSoftRigidDynamicsWorld.cpp
	btSoftMultiBodyDynamicsWorld.cpp
	btSoftSoftCollisionAlgorithm.cpp
	btDefaultSoftBodySolver.cpp

	btDeformableBackwardEulerObjective.cpp
	btDeformableBodySolver.cpp
	btDeformableMultiBodyConstraintSolver.cpp
	btDeformableContactProjection.cpp
	btDeformableMultiBodyDynamicsWorld.cpp
	btDeformableContactConstraint.cpp
	poly34.cpp

	BulletReducedDeformableBody/btReducedDeformableBody.cpp
	BulletReducedDeformableBody/btReducedDeformableBodyHelpers.cpp
	BulletReducedDeformableBody/btReducedDeformableBodySolver.cpp
	BulletReducedDeformableBody/btReducedDeformableContactConstraint.cpp
)


SET(BulletSoftBody_HDRS
	btSoftBody.h
	btSoftBodyData.h
	btSoftBodyConcaveCollisionAlgorithm.h
	btSoftBodyHelpers.h
	btSoftBodyRigidBodyCollisionConfiguration.h
	btSoftRigidCollisionAlgorithm.h
	btSoftRigidDynamicsWorld.h
	btSoftMultiBodyDynamicsWorld.h
	btSoftSoftCollisionAlgorithm.h
	btSparseSDF.h

	btSoftBodySolvers.h
	btDefaultSoftBodySolver.h
	
	btCGProjection.h
	btConjugateGradient.h
	btConjugateResidual.h
	btDeformableGravityForce.h
	btDeformableMassSpringForce.h
	btDeformableCorotatedForce.h
	btDeformableNeoHookeanForce.h
	btDeformableLinearElasticityForce.h
	btDeformableLagrangianForce.h
	btPreconditioner.h

	btDeformableBackwardEulerObjective.h
	btDeformableBodySolver.h
	btDeformableMultiBodyConstraintSolver.h
	btDeformableContactProjection.h
	btDeformableMultiBodyDynamicsWorld.h
	btDeformableContactConstraint.h
	btKrylovSolver.h
	poly34.h

	btSoftBodySolverVertexBuffer.h

	BulletReducedDeformableBody/btReducedDeformableBody.h
	BulletReducedDeformableBody/btReducedDeformableBodyHelpers.h
	BulletReducedDeformableBody/btReducedDeformableBodySolver.h
	BulletReducedDeformableBody/btReducedDeformableContactConstraint.h
)



ADD_LIBRARY(BulletSoftBody  ${BulletSoftBody_SRCS} ${BulletSoftBody_HDRS})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES SOVERSION ${BULLET_VERSION})
IF (BUILD_SHARED_LIBS)
	TARGET_LINK_LIBRARIES(BulletSoftBody BulletDynamics)
ENDIF (BUILD_SHARED_LIBS)
