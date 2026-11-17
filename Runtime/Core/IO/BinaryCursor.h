#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// Little-endian byte cursor. Integers and floats are stored in host byte order.
class BinaryWriter
{
public:
	void U8(uint8_t value)
	{
		m_bytes.push_back(value);
	}

	void U32(uint32_t value)
	{
		Append(&value, sizeof(value));
	}

	void U64(uint64_t value)
	{
		Append(&value, sizeof(value));
	}

	void I32(int32_t value)
	{
		Append(&value, sizeof(value));
	}

	void F32(float value)
	{
		Append(&value, sizeof(value));
	}

	void Bytes(const void* data, size_t size)
	{
		Append(data, size);
	}

	void String(std::string_view text)
	{
		U32(static_cast<uint32_t>(text.size()));
		Bytes(text.data(), text.size());
	}

	[[nodiscard]] const std::vector<uint8_t>& Data() const
	{
		return m_bytes;
	}

private:
	void Append(const void* data, size_t size)
	{
		const auto* raw = static_cast<const uint8_t*>(data);
		m_bytes.insert(m_bytes.end(), raw, raw + size);
	}

	std::vector<uint8_t> m_bytes{};
};

// Reads the bytes written by BinaryWriter. A failed read leaves Reading() false.
class BinaryReader
{
public:
	BinaryReader(const uint8_t* data, size_t size)
		: m_cursor(data)
		, m_end(data + size)
	{
	}

	[[nodiscard]] bool Reading() const
	{
		return m_ok;
	}

	// True when every byte was consumed and no read failed.
	[[nodiscard]] bool Exhausted() const
	{
		return m_ok && m_cursor == m_end;
	}

	void Fail()
	{
		m_ok = false;
	}

	[[nodiscard]] uint8_t U8()
	{
		return ReadPod<uint8_t>();
	}

	[[nodiscard]] uint32_t U32()
	{
		return ReadPod<uint32_t>();
	}

	[[nodiscard]] uint64_t U64()
	{
		return ReadPod<uint64_t>();
	}

	[[nodiscard]] int32_t I32()
	{
		return ReadPod<int32_t>();
	}

	[[nodiscard]] float F32()
	{
		return ReadPod<float>();
	}

	// A u32 length followed by that many bytes. Empty text is kept. Fails when the length exceeds maxLength.
	[[nodiscard]] std::string String(uint32_t maxLength)
	{
		const uint32_t length = U32();
		if (!m_ok || length > maxLength)
		{
			m_ok = false;
			return {};
		}

		if (length == 0)
		{
			return {};
		}

		Need(length);
		if (!m_ok)
		{
			return {};
		}

		std::string text(reinterpret_cast<const char*>(m_cursor), length);
		m_cursor += length;
		return text;
	}

	// A u32 count. Fails when the count exceeds maxCount.
	[[nodiscard]] uint32_t Count(uint32_t maxCount)
	{
		const uint32_t count = U32();
		if (!m_ok || count > maxCount)
		{
			m_ok = false;
			return 0;
		}

		return count;
	}

	// Copies size bytes. Zero bytes is an empty vector. Fails when size exceeds maxSize.
	[[nodiscard]] std::vector<std::byte> Bytes(uint32_t size, uint32_t maxSize)
	{
		if (!m_ok || size > maxSize)
		{
			m_ok = false;
			return {};
		}

		if (size == 0)
		{
			return {};
		}

		Need(size);
		if (!m_ok)
		{
			return {};
		}

		std::vector<std::byte> bytes(size);
		std::memcpy(bytes.data(), m_cursor, size);
		m_cursor += size;
		return bytes;
	}

private:
	template<typename T>
	[[nodiscard]] T ReadPod()
	{
		T value{};
		Need(sizeof(T));
		if (!m_ok)
		{
			return value;
		}

		std::memcpy(&value, m_cursor, sizeof(T));
		m_cursor += sizeof(T);
		return value;
	}

	void Need(size_t size)
	{
		if (!m_ok || static_cast<size_t>(m_end - m_cursor) < size)
		{
			m_ok = false;
		}
	}

	const uint8_t* m_cursor = nullptr;
	const uint8_t* m_end = nullptr;
	bool m_ok = true;
};
