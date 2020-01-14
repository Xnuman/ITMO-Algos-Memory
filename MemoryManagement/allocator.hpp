#ifndef ITMO_GAMEDEV_ALLOCATOR
#define ITMO_GAMEDEV_ALLOCATOR

#include <iostream>
#include <algorithm>
#include <cassert>
#include <vector>
#include <set>

using byte = unsigned char;

class MemoryAllocator {

public:

	virtual void init() = 0;
	virtual void destroy() = 0;
	virtual void* alloc(size_t size) = 0;
	virtual void free(void* p) = 0;
	virtual void dumpStat() const = 0;
	virtual void dumpBlocks() const = 0;

};

class FSA : MemoryAllocator {

public:
	FSA(size_t sizeOfBlock, size_t nBlocks);
	~FSA();

	void init() override;
	void destroy() override;
	void* alloc(size_t size) override;
	virtual void free(void* p) override;
	virtual void dumpStat() const override;
	virtual void dumpBlocks() const override;

private:
	byte* getAddresOfBlock(size_t n);
	size_t getOrderOfBlockByAddres(byte* addr);
	size_t nAlloc;
	size_t nFree;
	std::set<size_t > allocBlocks_;

	size_t sizeOfBlock_;
	size_t nBlocks_;
	size_t nUsed_;
	size_t nNextForAlloc;
	byte* pool_;

};

class CH : MemoryAllocator {
public:
	CH(size_t size);
	~CH();

	void init() override;
	void destroy() override;
	void* alloc(size_t size) override;
	virtual void free(void* p) override;
	virtual void dumpStat() const override;
	virtual void dumpBlocks() const override;
	byte* getDataPtr();
	void dumpCH(bool doSortByOffset = false) const;

private:
	struct Header {
		size_t id;
		size_t size;
		Header* prev;
		Header* next;
	};

	static size_t getOverheadBytes();
	static void* getUserPointer(const Header* hdr);
	static Header** getHeaderPointer(const Header* hdr);
	static Header* remove(Header* hdr);
	static void insertAfter(Header* after, Header* hdr);

	size_t nAlloc;
	size_t nFree;
	size_t size_;
	byte* data_;
	Header* freeList_;
};

#include "allocator_impl.hpp"

#endif
