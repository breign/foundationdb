/*
 * flat_buffers.cpp
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2018 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "flow/flat_buffers.h"
#include "flow/UnitTest.h"
#include "flow/Arena.h"
#include "flow/serialize.h"
#include "flow/ObjectSerializer.h"

#include <algorithm>
#include <iomanip>
#include <variant>

namespace detail {

namespace {
std::vector<int> mWriteToOffsetsMemoy;
}

std::vector<int>* writeToOffsetsMemory = &mWriteToOffsetsMemoy;

VTable generate_vtable(size_t numMembers, const std::vector<unsigned>& sizesAlignments) {
	if (numMembers == 0) {
		return VTable{ 4, 4 };
	}
	// first is index, second is size
	std::vector<std::pair<unsigned, unsigned>> indexed;
	indexed.reserve(numMembers);
	for (unsigned i = 0; i < numMembers; ++i) {
		if (sizesAlignments[i] > 0) {
			indexed.emplace_back(i, sizesAlignments[i]);
		}
	}
	std::stable_sort(indexed.begin(), indexed.end(),
	                 [](const std::pair<unsigned, unsigned>& lhs, const std::pair<unsigned, unsigned>& rhs) {
		                 return lhs.second > rhs.second;
	                 });
	VTable result;
	result.resize(numMembers + 2);
	// size of the vtable is
	// - 2 bytes per member +
	// - 2 bytes for the size entry +
	// - 2 bytes for the size of the object
	result[0] = 2 * numMembers + 4;
	int offset = 0;
	for (auto p : indexed) {
		auto align = sizesAlignments[numMembers + p.first];
		auto& res = result[p.first + 2];
		res = offset % align == 0 ? offset : ((offset / align) + 1) * align;
		offset = res + p.second;
		res += 4;
	}
	result[1] = offset + 4;
	return result;
}

} // namespace detail

namespace unit_tests {

TEST_CASE("flow/FlatBuffers/test") {
	auto* vtable1 = detail::get_vtable<int>();
	auto* vtable2 = detail::get_vtable<uint8_t, uint8_t, int, int64_t, int>();
	auto* vtable3 = detail::get_vtable<uint8_t, uint8_t, int, int64_t, int>();
	auto* vtable4 = detail::get_vtable<uint32_t>();
	ASSERT(vtable1 != vtable2);
	ASSERT(vtable2 == vtable3);
	ASSERT(vtable1 == vtable4); // Different types, but same vtable! Saves space in encoded messages
	ASSERT(vtable1->size() == 3);
	ASSERT(vtable2->size() == 7);
	ASSERT((*vtable2)[0] == 14);
	ASSERT((*vtable2)[1] == 22);
	ASSERT(((*vtable2)[4] - 4) % 4 == 0);
	ASSERT(((*vtable2)[5] - 4) % 8 == 0);
	ASSERT(((*vtable2)[6] - 4) % 4 == 0);
	return Void();
}

TEST_CASE("flow/FlatBuffers/emptyVtable") {
	auto* vtable = detail::get_vtable<>();
	ASSERT((*vtable)[0] == 4);
	ASSERT((*vtable)[1] == 4);
	return Void();
}

struct Table2 {
	std::string m_p = {};
	bool m_ujrnpumbfvc = {};
	int64_t m_iwgxxt = {};
	int64_t m_tjkuqo = {};
	int16_t m_ed = {};
	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, m_p, m_ujrnpumbfvc, m_iwgxxt, m_tjkuqo, m_ed);
	}
};

struct Table3 {
	uint16_t m_asbehdlquj = {};
	uint16_t m_k = {};
	uint16_t m_jib = {};
	int64_t m_n = {};
	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, m_asbehdlquj, m_k, m_jib, m_n);
	}
};

TEST_CASE("flow/FlatBuffers/vtable2") {
	const auto& vtable =
	    *detail::get_vtable<uint64_t, bool, std::string, int64_t, std::vector<uint16_t>, Table2, Table3>();
	ASSERT(!(vtable[2] <= vtable[4] && vtable[4] < vtable[2] + 8));
	return Void();
}

struct Nested2 {
	uint8_t a;
	std::vector<std::string> b;
	int c;
	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, a, b, c);
	}

	friend bool operator==(const Nested2& lhs, const Nested2& rhs) {
		return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c;
	}
};

struct Nested {
	uint8_t a;
	std::string b;
	Nested2 nested;
	std::vector<uint64_t> c;
	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, a, b, nested, c);
	}
};

struct Root {
	uint8_t a;
	std::vector<Nested2> b;
	Nested c;
	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, a, b, c);
	}
};

struct TestContextArena {
	Arena& _arena;
	Arena& arena() { return _arena; }
	ProtocolVersion protocolVersion() const { return currentProtocolVersion; }
	uint8_t* allocate(size_t size) { return new (_arena) uint8_t[size]; }
};

TEST_CASE("flow/FlatBuffers/collectVTables") {
	Root root;
	Arena arena;
	TestContextArena context{ arena };
	const auto* vtables = detail::get_vtableset(root, context);
	ASSERT(vtables == detail::get_vtableset(root, context));
	const auto& root_vtable = *detail::get_vtable<uint8_t, std::vector<Nested2>, Nested>();
	const auto& nested_vtable = *detail::get_vtable<uint8_t, std::vector<std::string>, int>();
	int root_offset = vtables->getOffset(&root_vtable);
	int nested_offset = vtables->getOffset(&nested_vtable);
	ASSERT(!memcmp((uint8_t*)&root_vtable[0], &vtables->packed_tables[root_offset], root_vtable.size()));
	ASSERT(!memcmp((uint8_t*)&nested_vtable[0], &vtables->packed_tables[nested_offset], nested_vtable.size()));
	return Void();
}

void print_buffer(const uint8_t* out, int len) {
	std::cout << std::hex << std::setfill('0');
	for (int i = 0; i < len; ++i) {
		if (i % 8 == 0) {
			std::cout << std::endl;
			std::cout << std::setw(4) << i << ": ";
		}
		std::cout << std::setw(2) << (int)out[i] << " ";
	}
	std::cout << std::endl << std::dec;
}

struct Arena {
	std::vector<std::pair<uint8_t*, size_t>> allocated;
	~Arena() {
		for (auto b : allocated) {
			delete[] b.first;
		}
	}

	uint8_t* operator()(size_t sz) {
		auto res = new uint8_t[sz];
		allocated.emplace_back(res, sz);
		return res;
	}

	size_t get_size(const uint8_t* ptr) const {
		for (auto& p : allocated) {
			if (p.first == ptr) {
				return p.second;
			}
		}
		return -1;
	}
};

struct TestContext {
	Arena& _arena;
	Arena& arena() { return _arena; }
	ProtocolVersion protocolVersion() const { return currentProtocolVersion; }
	uint8_t* allocate(size_t size) { return _arena(size); }
	TestContext& context() { return *this; }
};

TEST_CASE("flow/FlatBuffers/serializeDeserializeRoot") {
	Root root{ 1,
		       { { 13, { "ghi", "jkl" }, 15 }, { 16, { "mnop", "qrstuv" }, 18 } },
		       { 3, "hello", { 6, { "abc", "def" }, 8 }, { 10, 11, 12 } } };
	Root root2 = root;
	Arena arena;
	TestContext context{ arena };
	auto out = detail::save(context, root, FileIdentifier{});

	ASSERT(root.a == root2.a);
	ASSERT(root.b == root2.b);
	ASSERT(root.c.a == root2.c.a);
	ASSERT(root.c.b == root2.c.b);
	ASSERT(root.c.nested.a == root2.c.nested.a);
	ASSERT(root.c.nested.b == root2.c.nested.b);
	ASSERT(root.c.nested.c == root2.c.nested.c);
	ASSERT(root.c.c == root2.c.c);

	root2 = {};
	detail::load(root2, out, context);

	ASSERT(root.a == root2.a);
	ASSERT(root.b == root2.b);
	ASSERT(root.c.a == root2.c.a);
	ASSERT(root.c.b == root2.c.b);
	ASSERT(root.c.nested.a == root2.c.nested.a);
	ASSERT(root.c.nested.b == root2.c.nested.b);
	ASSERT(root.c.nested.c == root2.c.nested.c);
	ASSERT(root.c.c == root2.c.c);
	return Void();
}

TEST_CASE("flow/FlatBuffers/serializeDeserializeMembers") {
	Root root{ 1,
		       { { 13, { "ghi", "jkl" }, 15 }, { 16, { "mnop", "qrstuv" }, 18 } },
		       { 3, "hello", { 6, { "abc", "def" }, 8 }, { 10, 11, 12 } } };
	Root root2 = root;
	Arena arena;
	TestContext context{arena};
	const auto* out = save_members(context, FileIdentifier{}, root.a, root.b, root.c);

	ASSERT(root.a == root2.a);
	ASSERT(root.b == root2.b);
	ASSERT(root.c.a == root2.c.a);
	ASSERT(root.c.b == root2.c.b);
	ASSERT(root.c.nested.a == root2.c.nested.a);
	ASSERT(root.c.nested.b == root2.c.nested.b);
	ASSERT(root.c.nested.c == root2.c.nested.c);
	ASSERT(root.c.c == root2.c.c);

	root2 = {};
	load_members(out, context, root2.a, root2.b, root2.c);

	ASSERT(root.a == root2.a);
	ASSERT(root.b == root2.b);
	ASSERT(root.c.a == root2.c.a);
	ASSERT(root.c.b == root2.c.b);
	ASSERT(root.c.nested.a == root2.c.nested.a);
	ASSERT(root.c.nested.b == root2.c.nested.b);
	ASSERT(root.c.nested.c == root2.c.nested.c);
	ASSERT(root.c.c == root2.c.c);
	return Void();
}

} // namespace unit_tests

namespace unit_tests {

TEST_CASE("flow/FlatBuffers/variant") {
	using V = boost::variant<int, double, Nested2>;
	V v1;
	V v2;
	Arena arena;
	TestContext context{arena};
	const uint8_t* out;

	v1 = 1;
	out = save_members(context, FileIdentifier{}, v1);
	// print_buffer(out, arena.get_size(out));
	load_members(out, context, v2);
	ASSERT(boost::get<int>(v1) == boost::get<int>(v2));

	v1 = 1.0;
	out = save_members(context, FileIdentifier{}, v1);
	// print_buffer(out, arena.get_size(out));
	load_members(out, context, v2);
	ASSERT(boost::get<double>(v1) == boost::get<double>(v2));

	v1 = Nested2{ 1, { "abc", "def" }, 2 };
	out = save_members(context, FileIdentifier{}, v1);
	// print_buffer(out, arena.get_size(out));
	load_members(out, context, v2);
	ASSERT(boost::get<Nested2>(v1).a == boost::get<Nested2>(v2).a);
	ASSERT(boost::get<Nested2>(v1).b == boost::get<Nested2>(v2).b);
	ASSERT(boost::get<Nested2>(v1).c == boost::get<Nested2>(v2).c);
	return Void();
}

TEST_CASE("flow/FlatBuffers/vectorBool") {
	std::vector<bool> x1 = { true, false, true, false, true };
	std::vector<bool> x2;
	Arena arena;
	TestContext context{arena};
	const uint8_t* out;

	out = save_members(context, FileIdentifier{}, x1);
	// print_buffer(out, arena.get_size(out));
	load_members(out, context, x2);
	ASSERT(x1 == x2);
	return Void();
}

} // namespace unit_tests

namespace unit_tests {

struct Y1 {
	int a;

	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, a);
	}
};

struct Y2 {
	int a;
	boost::variant<int> b;

	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, a, b);
	}
};

template <class Y>
struct X {
	int a;
	Y b;
	int c;

	template <class Archiver>
	void serialize(Archiver& ar) {
		serializer(ar, a, b, c);
	}
};

TEST_CASE("/flow/FlatBuffers/nestedCompat") {
	X<Y1> x1 = { 1, { 2 }, 3 };
	X<Y2> x2;
	Arena arena;
	TestContext context{arena};
	const uint8_t* out;

	out = save_members(context, FileIdentifier{}, x1);
	load_members(out, context, x2);
	ASSERT(x1.a == x2.a);
	ASSERT(x1.b.a == x2.b.a);
	ASSERT(x1.c == x2.c);

	x1 = {};
	x2.b.b = 4;

	out = save_members(context, FileIdentifier{}, x2);
	load_members(out, context, x1);
	ASSERT(x1.a == x2.a);
	ASSERT(x1.b.a == x2.b.a);
	ASSERT(x1.c == x2.c);
	return Void();
}

TEST_CASE("/flow/FlatBuffers/struct") {
	std::vector<std::tuple<int16_t, bool, int64_t>> x1 = { { 1, true, 2 }, { 3, false, 4 } };
	decltype(x1) x2;
	Arena arena;
	TestContext context{arena };
	const uint8_t* out;

	out = save_members(context, FileIdentifier{}, x1);
	// print_buffer(out, arena.get_size(out));
	load_members(out, context, x2);
	ASSERT(x1 == x2);
	return Void();
}

TEST_CASE("/flow/FlatBuffers/file_identifier") {
	Arena arena;
	TestContext context{arena};
	const uint8_t* out;
	constexpr FileIdentifier file_identifier{ 1234 };
	out = save_members(context, file_identifier);
	// print_buffer(out, arena.get_size(out));
	ASSERT(read_file_identifier(out) == file_identifier);
	return Void();
}

TEST_CASE("/flow/FlatBuffers/VectorRef") {
	// this test tests a few weird memory properties of
	// serialized arenas. This is why it uses weird scoping

	// first we construct the data to serialize/deserialize
	// so we can compare it afterwards
	std::vector<std::string> src;
	src.push_back("Foo");
	src.push_back("Bar");
	::Arena vecArena;
	VectorRef<StringRef> outVec;
	{
		::Arena readerArena;
		StringRef serializedVector;
		{
			::Arena arena;
			VectorRef<StringRef> vec;
			for (const auto& str : src) {
				vec.push_back(arena, str);
			}
			ObjectWriter writer(Unversioned());
			writer.serialize(FileIdentifierFor<decltype(vec)>::value, arena, vec);
			serializedVector = StringRef(readerArena, writer.toStringRef());
		}
		ArenaObjectReader reader(readerArena, serializedVector, Unversioned());
		// The VectorRef and Arena arguments are intentionally in a different order from the serialize call above.
		// Arenas need to get serialized after any Ref types whose memory they own. In order for schema evolution to be
		// possible, it needs to be okay to reorder an Arena so that it appears after a newly added Ref type. For this
		// reason, Arenas are ignored by the wire protocol entirely. We test that behavior here.
		reader.deserialize(FileIdentifierFor<decltype(outVec)>::value, outVec, vecArena);
	}
	ASSERT(src.size() == outVec.size());
	for (int i = 0; i < src.size(); ++i) {
		auto str = outVec[i].toString();
		ASSERT(str == src[i]);
	}
	return Void();
}

TEST_CASE("/flow/FlatBuffers/Standalone") {
	Standalone<VectorRef<StringRef>> vecIn;
	auto numElements = deterministicRandom()->randomInt(1, 20);
	for (int i = 0; i < numElements; ++i) {
		auto str = deterministicRandom()->randomAlphaNumeric(deterministicRandom()->randomInt(0, 30));
		vecIn.push_back(vecIn.arena(), StringRef(vecIn.arena(), str));
	}
	Standalone<StringRef> value = ObjectWriter::toValue(vecIn, Unversioned());
	ArenaObjectReader reader(value.arena(), value, Unversioned());
	VectorRef<Standalone<StringRef>> vecOut;
	reader.deserialize(vecOut);
	ASSERT(vecOut.size() == vecIn.size());
	for (int i = 0; i < vecOut.size(); ++i) {
		ASSERT(vecOut[i] == vecIn[i]);
	}
	return Void();
}

} // namespace unit_tests
