#pragma once

#include "core/io/resource.h"
class SceneState;
class SceneStateInstanceGetter {
public:
	static Ref<Resource> get_fake_instance(SceneState *state, int p_idx);
};