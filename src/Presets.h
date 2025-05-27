#pragma once

#include "dsp/Pattern.h"
#include <vector>
#include <string>
#include <sstream>

class Presets {
public:
	static std::vector<PPoint> getCutPreset(int index) {
		const auto& presets = getCutPresets();
		if (index < 0 || index >= static_cast<int>(presets.size()))
			return {};
		return parsePreset(presets[index]);
	}

	static std::vector<PPoint> getResPreset(int index) {
		const auto& presets = getResPresets();
		if (index < 0 || index >= static_cast<int>(presets.size()))
			return {};
		return parsePreset(presets[index]);
	}

	static std::vector<PPoint> getPaintPreset(int index) {
		const auto& presets = getPaintPresets();
		if (index < 0 || index >= static_cast<int>(presets.size()))
			return {};
		return parsePreset(presets[index]);
	}

private:
	static const std::vector<std::string>& getPaintPresets() {
		static const std::vector<std::string> presets = {
			"0 1 0 1 1 1 0 1", // empty / delete
			"0 1 0 1 1 0 0 1", // line
			"0.005 1 0 1 0.995 0 0 1", // Saw
			"0 1 0 1 0.5 0 0 1 1 1 0 1", // triangle
			"0.005 1 0 3 0.995 0 0 1", // square
			"0.005 1 0.35 2 0.995 0 0 1", // S-Curve
			"0.25 1 0.2 2 0.75 0 0.2 2", // sine
			"0 1 0 1 0 0 -0.25 1 0.25 1 0 1 0.25 0.375 -0.25 1 0.5 1 0 1 0.5 0 -0.25 1 0.75 1 0 1 0.75 0.375 -0.25 1 1 1 0 1", // waves
		};
		return presets;
	}

	static const std::vector<std::string>& getCutPresets() {
		static const std::vector<std::string> presets = {
			"", // HEAD
			"", // EMPTY
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",

			"", // HEAD
			"", 
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
		    
			"", // HEAD
			"", 
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
		};
		return presets;
	}

	static const std::vector<std::string>& getResPresets() {
		static const std::vector<std::string> presets = {
			"", // HEAD
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",

			"", // HEAD
			"", 
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",

			"", // HEAD
			"", 
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
			"",
		};
		return presets;
	}

	static std::vector<PPoint> parsePreset(const std::string& str) {
		std::vector<PPoint> result;
		std::istringstream iss(str);
		double x, y, tension;
		int type;

		if (str.empty()) {
			return {};  // Return empty array if the string is empty
		}

		while (iss >> x >> y >> tension >> type) {
			result.emplace_back(PPoint{0, x, y, tension, type});
		}

		return result;
	}
};