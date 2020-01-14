#ifndef ITMO_GAMEDEV_ALLOCATOR_IMPL
#define ITMO_GAMEDEV_ALLOCATOR_IMPL

#include "allocator.hpp"

void FSA::init() {
	pool_ = new byte[nBlocks_ * sizeOfBlock_];
	for (size_t i = 0; i < nBlocks_ - 1; ++i) {
		size_t* pBlock = (size_t*)getAddresOfBlock(i);
		*pBlock = i + 1;
	}
}

FSA::FSA(size_t sizeOfBlock, size_t nBlocks) : sizeOfBlock_(sizeOfBlock), nBlocks_(nBlocks), nNextForAlloc(0), pool_(nullptr), nUsed_(0) {
	sizeOfBlock_ = std::max(sizeOfBlock_, sizeof(size_t));
	nAlloc = 0;
	nFree = 0;
};

FSA::~FSA() {
	destroy();
};


void FSA::destroy() {
	delete[] pool_;
};

void* FSA::alloc(size_t size) {
	if (size > sizeOfBlock_ || nUsed_ >= nBlocks_) {
		return nullptr;
	}

	byte* retPtr = getAddresOfBlock(nNextForAlloc);
	nNextForAlloc = *(size_t*)retPtr;
	++nUsed_;
	++nAlloc;
	allocBlocks_.insert(getOrderOfBlockByAddres(retPtr));

	return retPtr;
};

void FSA::free(void* p) {
	*(size_t*)p = nNextForAlloc;
	nNextForAlloc = *(size_t*)p;
	--nUsed_;
	++nFree;
	allocBlocks_.erase(getOrderOfBlockByAddres((byte*)p));
};


void FSA::dumpBlocks() const {
	printf("dump stats after %lu alloc, %lu free\n", nAlloc, nFree);
	for (size_t i = 0; i < nBlocks_; ++i) {
		bool isFree = !allocBlocks_.count(i);
		if (isFree) {
			printf("--- |--%lu--|\n", sizeOfBlock_);
		}
		else {
			printf("+++ |--%lu--|\n", sizeOfBlock_);
		}
	}
};
void FSA::dumpStat() const {
	printf("dump stats after %lu alloc, %lu free\n", nAlloc, nFree);
	for (size_t i = 0; i < nBlocks_; ++i) {
		bool isFree = !allocBlocks_.count(i);
		if (isFree) {
			printf("+++ |--%lu--|\n", sizeOfBlock_);
		}
	}
};



byte* FSA::getAddresOfBlock(size_t n) {
	assert(n < nBlocks_);
	return pool_ + n * sizeOfBlock_;
};

size_t FSA::getOrderOfBlockByAddres(byte* addr) {
	assert((size_t)(addr - pool_) % sizeOfBlock_ == 0);
	return (size_t)(addr - pool_) / sizeOfBlock_;
};

CH::CH(size_t size) : size_(size) {
	assert(size >= getOverheadBytes());
	nAlloc = 0;
	nFree = 0;
};

CH::~CH() {
	destroy();
};

void CH::init() {
	data_ = new byte[size_];

	freeList_ = (Header*)data_;
	freeList_->size = size_ - getOverheadBytes();
	freeList_->prev = freeList_;
	freeList_->next = freeList_;
};

void CH::destroy() {
	delete[] data_;
};

void* CH::alloc(size_t size) {
	if (freeList_) {
		const Header* startHdr = freeList_;
		Header* iterBlockHdr = freeList_;
		do {
			if (iterBlockHdr->size >= size) {
				freeList_ = remove(iterBlockHdr);

				if (iterBlockHdr->size > size + getOverheadBytes()) {
					Header* newBlockHdr = (Header*)((byte*)iterBlockHdr + getOverheadBytes() + size);
					newBlockHdr->size = iterBlockHdr->size - getOverheadBytes() - size;
					*getHeaderPointer(newBlockHdr) = newBlockHdr;

					iterBlockHdr->size = size;
					*getHeaderPointer(iterBlockHdr) = nullptr;

					if (freeList_) {
						insertAfter(freeList_, newBlockHdr);
					}
					else {
						newBlockHdr->prev = newBlockHdr;
						newBlockHdr->next = newBlockHdr;
						freeList_ = newBlockHdr;
					}
				}

				iterBlockHdr->id = nAlloc;
				++nAlloc;
				return getUserPointer(iterBlockHdr);
			}
			iterBlockHdr = iterBlockHdr->next;
		} while (iterBlockHdr == startHdr);
	}

	return nullptr;
};

void CH::free(void* p) {
	Header* removeBlockHdr = (Header*)((byte*)p - sizeof(Header));
	if (freeList_) {
		byte* addrPrevBlockFooter = (byte*)removeBlockHdr - sizeof(Header);
		Header** prevBlockFooter = (Header**)(addrPrevBlockFooter);
		if (addrPrevBlockFooter > data_&&* prevBlockFooter) {
			Header* prevBlockHdr = *prevBlockFooter;
			prevBlockHdr->size += getOverheadBytes() + removeBlockHdr->size;
			freeList_ = prevBlockHdr;
		}
		else {
			insertAfter(freeList_, removeBlockHdr);
		}

		byte* addrNextBlockHdr = (byte*)getHeaderPointer(removeBlockHdr) + sizeof(Header*);
		Header* nextBlockHdr = (Header*)(addrNextBlockHdr);
		if (addrNextBlockHdr < data_ + size_ && *getHeaderPointer(nextBlockHdr)) {
			remove(nextBlockHdr);
			removeBlockHdr->size += getOverheadBytes() + nextBlockHdr->size;
			freeList_ = removeBlockHdr;
		}
	}
	else {
		removeBlockHdr->prev = removeBlockHdr;
		removeBlockHdr->next = removeBlockHdr;
		freeList_ = removeBlockHdr;
	}
	++nFree;
};

void CH::dumpStat() const {
	std::set<Header*> hdrs;
	if (freeList_) {
		const Header* startBlockHdr = freeList_;
		Header* iterBlockHdr = freeList_;
		do {
			hdrs.insert(iterBlockHdr);
			iterBlockHdr = iterBlockHdr->next;
		} while (iterBlockHdr != startBlockHdr);
	}

	printf("dump stats after %lu alloc, %lu free\n", nAlloc, nFree);

	Header* iterBlock = (Header*)data_;
	while ((byte*)iterBlock < data_ + size_) {
		bool isFree = hdrs.count(iterBlock);
		if (isFree) {
			printf("--- |--%lu--|\n", iterBlock->size);
		}
		else {
			printf("+++ |--%lu--| id: %lu\n", iterBlock->size, iterBlock->id);
		}

		iterBlock = (Header*)((byte*)iterBlock + getOverheadBytes() + iterBlock->size);
	}
};

void CH::dumpBlocks() const {
	if (freeList_) {
		const Header* startBlockHdr = freeList_;

		std::vector<Header*> hdrs;
		Header* iterBlockHdr = freeList_;
		do {
			hdrs.emplace_back(iterBlockHdr);
			iterBlockHdr = iterBlockHdr->next;
		} while (iterBlockHdr != startBlockHdr);

		std::sort(hdrs.begin(), hdrs.end(), [](const Header* r, const Header* l) { return r < l; });

		printf("alloc dump after %lu alloc, %lu free\n", nAlloc, nFree);
		for (const auto& hdr : hdrs) {
			printf("alloc addr: %p, size: %lu\n", hdr, hdr->size);
		}
	}
};

byte* CH::getDataPtr() {
	return data_;
};

void CH::dumpCH(bool doSortByOffset) const {
	printf("dump free list after %lu alloc, %lu free\n", nAlloc, nFree);
	if (freeList_) {
		const Header* startBlockHdr = freeList_;

		std::vector<Header*> hdrs;
		Header* iterBlockHdr = freeList_;
		do {
			hdrs.emplace_back(iterBlockHdr);
			iterBlockHdr = iterBlockHdr->next;
		} while (iterBlockHdr != startBlockHdr);

		if (doSortByOffset) {
			std::sort(hdrs.begin(), hdrs.end(), [](const Header* r, const Header* l) { return r < l; });
		}

		for (const auto& hdr : hdrs) {
			printf(" { offset: %lu, size: %lu }\n", (size_t)((byte*)hdr - data_), hdr->size);
		}
	}
	else {
		printf("free list is empty\n");
	}
};

size_t CH::getOverheadBytes() {
	return sizeof(Header) + sizeof(Header*);
};

void* CH::getUserPointer(const Header* hdr) {
	return (void*)((byte*)hdr + sizeof(Header));
};

CH::Header** CH::getHeaderPointer(const Header* hdr) {
	return (Header**)((byte*)hdr + sizeof(Header) + hdr->size);
};

void CH::insertAfter(Header* after, Header* hdr) {
	after->next->prev = hdr;
	hdr->next = after->next;
	after->next = hdr;
	hdr->prev = after;
};

CH::Header* CH::remove(Header* hdr) {
	if (hdr->prev == hdr) {
		assert(hdr->next == hdr);
		return nullptr;
	}
	else {
		hdr->prev->next = hdr->next;
		hdr->next->prev = hdr->prev;
		return hdr->prev;
	}
};



#endif
