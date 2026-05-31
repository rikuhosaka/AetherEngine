#pragma once

#include <vector>

struct ExtractedObject;

class ISceneExtractor
{
public:
	virtual ~ISceneExtractor() = default;
	virtual void Extract(std::vector<ExtractedObject>& outObjects) = 0;
};
