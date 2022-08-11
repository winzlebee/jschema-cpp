# Creates the executable that creates *.h, *.cpp files based
# on a json schema

SOURCE_FILES := main.cpp
COMPILE_FLAGS := -I extern -ojschema-cpp -g

jschema-cpp : $(SOURCE_FILES)
	$(CXX) $(COMPILE_FLAGS) $(SOURCE_FILES)