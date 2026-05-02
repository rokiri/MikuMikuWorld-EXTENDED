#pragma once
#include <stdint.h>
#include <array>
#include <string>
#include "ImGui/imgui.h"
#include "Utilities.h"

struct InputBinding
{
	int keyCode{};
	int keyModifiers{};

	InputBinding(int key, int mods) : keyCode(key), keyModifiers(mods) {}

	InputBinding(ImGuiKey key = ImGuiKey_None, ImGuiKey mods = ImGuiMod_None)
	    : keyCode{ key }, keyModifiers{ mods }
	{
	}

	constexpr bool operator==(const InputBinding& other) const
	{
		return keyModifiers == other.keyModifiers && keyCode == other.keyCode;
	}

	constexpr bool operator!=(const InputBinding& other) const { return !(*this == other); }

	constexpr inline explicit operator ImGuiKeyChord() const { return keyCode | keyModifiers; }
};

// DECLARE_ENUM_FLAG_OPERATORS(ImGuiKey);

struct MultiInputBinding
{
	int count;
	std::string_view name;
	std::array<InputBinding, 4> bindings;

	MultiInputBinding(std::string_view name) : name(name), count(0) {}

	MultiInputBinding(std::string_view name, InputBinding binding)
	    : name(name), count(binding.keyCode != 0), bindings{ binding }
	{
	}

	MultiInputBinding(std::string_view name, InputBinding b1, InputBinding b2)
	    : name(name), count(0)
	{
		if (b1.keyCode != 0)
			bindings[count++] = b1;

		if (b2.keyCode != 0)
			bindings[count++] = b2;
	}

	void moveUp(int index)
	{
		if (index <= 0 || index >= count)
			return;

		std::swap(bindings[index - 1], bindings[index]);
	}

	void moveDown(int index)
	{
		if (index < 0 || index + 1 >= count)
			return;

		std::swap(bindings[index + 1], bindings[index]);
	}

	void addBinding(InputBinding binding)
	{
		if (count == bindings.size() || binding.keyCode != 0)
			return;

		bindings.at(count++) = binding;
	}

	void removeAt(int index)
	{
		if (index < 0 || index >= count)
			return;

		// shift elements to the left
		std::rotate(bindings.begin() + index, bindings.begin() + index + 1,
		            bindings.begin() + count);
		--count;
	}
};

const char* ToShortcutString(const MultiInputBinding& binding);
const char* ToShortcutString(const InputBinding& binding);
const char* ToShortcutString(ImGuiKey key, ImGuiKey mods);
const char* ToShortcutString(ImGuiKey key);

std::string ToFullShortcutsString(const MultiInputBinding& binding);
std::string ToSerializedString(const InputBinding& binding);
InputBinding FromSerializedString(std::string string);

namespace ImGui
{
	bool TestModifiers(ImGuiKey mods);

	bool IsDown(const InputBinding& binding);
	bool IsPressed(const InputBinding& binding, bool repeat = false);

	bool IsAnyDown(const MultiInputBinding& binding);
	bool IsAnyPressed(const MultiInputBinding& binding, bool repeat = false);

	bool Shortcut(const InputBinding& binding, ImGuiInputFlags flags = ImGuiInputFlags_None);
	bool AnyShortcut(const MultiInputBinding& bindings,
	                 ImGuiInputFlags flags = ImGuiInputFlags_None);
}