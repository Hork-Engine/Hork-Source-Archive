
INCLUDE_DIRECTORIES(
	${BULLET_PHYSICS_SOURCE_DIR}/src
)

SET(Bullet3Geometry_SRCS
	b3ConvexHullComputer.cpp
	b3GeometryUtil.cpp
)

SET(Bullet3Geometry_HDRS
	b3AabbUtil.h
	b3ConvexHullComputer.h
	b3GeometryUtil.h
	b3GrahamScan2dConvexHull.h
)

ADD_LIBRARY(Bullet3Geometry ${Bullet3Geometry_SRCS} ${Bullet3Geometry_HDRS})
if (BUILD_SHARED_LIBS)
  target_link_libraries(Bullet3Geometry Bullet3Common)
endif()
SET_TARGET_PROPERTIES(Bullet3Geometry PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet3Geometry PROPERTIES SOVERSION ${BULLET_VERSION})
