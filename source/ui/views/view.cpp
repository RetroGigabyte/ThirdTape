#include <functional>
#include "ui/views/view.hpp"
#include "ui/colors.hpp"
#include "variables.hpp"
#include <algorithm>

const std::function<u32(const View &)> View::STANDARD_BACKGROUND = [](const View &view) {
	float t = (float)view.touch_darkness;
	if (view.touch_darkness > 0 && view.touch_darkness < 1) {
		var_need_refresh = true;
	}
	if (var_night_mode) {
		int g = (int)(0x00 + 0x2A * t);
		return COLOR_GRAY(g);
	}
	// white -> light green while the row is pressed
	int r = (int)(0xFF + (0xCF - 0xFF) * t), g = (int)(0xFF + (0xEE - 0xFF) * t), b = (int)(0xFF + (0xCF - 0xFF) * t);
	return COLOR_RGB(r, g, b);
};
