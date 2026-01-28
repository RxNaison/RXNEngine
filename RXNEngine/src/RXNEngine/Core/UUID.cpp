#include "rxnpch.h"
#include "UUID.h"

#include <random>

namespace RXNEngine {

	static std::random_device s_RandomDevice;
	static std::mt19937_64 s_Engine(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

	UUID::UUID()
	{
		uint64_t uuid = s_UniformDistribution(s_Engine);

		while (uuid == 0)
		{
			uuid = s_UniformDistribution(s_Engine);
		}

		m_UUID = uuid;
	}

	UUID::UUID(uint64_t uuid)
		: m_UUID(uuid)
	{
	}

	const UUID UUID::Null = UUID(0);

}