AM_CPPFLAGS= -fPIC -fpermissive -DTHERON_XS\
-I${THERON_HOME}/Include \
-I${BOOST_HOME} \
-I${HADOOP_HOME}/src/c++/libhdfs\
-I${JAVA_HOME}/include\
-I${JAVA_HOME}/include/linux

AM_LDFLAGS=-lc -lm -lrt -lconfig++ -lpthread -lboost_serialization -lxs

LDADD = 	../../Catalog/libcatalog.a \
		../../common/libcommon.a \
		../../common/Schema/libschema.a \
		../../utility/libutility.a \
		../../parsetree/libparsetree.a \
		${THERON_HOME}/Lib/libtherond.a \
		${BOOST_HOME}/stage/lib/libboost_serialization.a \
		${BOOST_HOME}/stage/lib/libboost_system.a \
		${GTEST_HOME}/libgtest.a

noinst_LIBRARIES=libexpression.a

libexpression_a_SOURCES = 	\
	execfunc.cpp 		execfunc.h \
	expression_test.h 	initquery.cpp \
	initquery.h 		qnode.cpp \
	qnode.h 			queryfunc.cpp \
	queryfunc.h
