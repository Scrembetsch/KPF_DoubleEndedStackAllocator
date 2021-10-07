/**
* Exercise: "DoubleEndedStackAllocator with Canaries" OR "Growing DoubleEndedStackAllocator with Canaries (VMEM)"
* Group members: Handls Anja (gs20m005), Tributsch Harald (gsgs20m008), Leithner Michael (gs20m012)
**/

#include "stdio.h"

#include <cassert>

namespace Tests
{
	void Test_Case_Success(const char* name, bool passed)
	{
		if (passed)
		{
			printf("[%s] passed the test!\n", name);
		}
		else
		{
			printf("[%s] failed the test!\n", name);
		}
	}

	void Test_Case_Failure(const char* name, bool passed)
	{
		if (!passed)
		{
			printf("[%s] passed the test!\n", name);
		}
		else
		{
			printf("[%s] failed the test!\n", name);
		}
	}

	/**
	* Example of how a test case can look like. The test cases in the end will check for
	* allocation success, proper alignment, overlaps and similar situations. This is an
	* example so you can already try to cover all cases you judge as being important by
	* yourselves.
	**/
	template<class A>
	bool VerifyAllocationSuccess(A& allocator, size_t size, size_t alignment)
	{
		void* mem = allocator.Allocate(size, alignment);

		if (mem == nullptr)
		{
			printf("[Error]: Allocator returned nullptr!\n");
			return false;
		}

		return true;
	}
}

// Assignment functionality tests are going to be included here

#define WITH_DEBUG_CANARIES 0

/**
* You work on your DoubleEndedStackAllocator. Stick to the provided interface, this is
* necessary for testing your assignment in the end. Don't remove or rename the public
* interface of the allocator. Also don't add any additional initialization code, the
* allocator needs to work after it was created and its constructor was called. You can
* add additional public functions but those should only be used for your own testing.
**/
class DoubleEndedStackAllocator
{
public:
	DoubleEndedStackAllocator(size_t max_size) {}

	void* Allocate(size_t size, size_t alignment)
	{
		assert(IsPowerOf2(alignment));

		return nullptr;
	}
	void* AllocateBack(size_t size, size_t alignment)
	{
		return nullptr;
	}

	void Free(void* memory) {}
	void FreeBack(void* memory) {}

	void Reset(void) {}

	~DoubleEndedStackAllocator(void) {}

private:
	// power of 2 always has exactly 1 bit set in binary representation (for signed values)
	bool IsPowerOf2(size_t val)
	{
		return val > 0 && !(val & (val - 1));
	}
};

/** TODO
* - reserve (virtual?) memory to be able to grow
*		AlignUp/Down utility functions
* - ability to grow
*		just request new memory and copy?
* - copy and move ctor/operator?
* - allocate
*		throw if overlap, invalid alignment, out of bounds, ...
*		-> assert? throw? just msg and return nullptr?
**/


int main()
{
	// You can add your own tests here, I will call my tests at then end with a fresh instance of your allocator and a specific max_size
	{
		// You can remove this, just showcasing how the test functions can be used
		DoubleEndedStackAllocator allocator(1024u);
		Tests::Test_Case_Success("Allocate() returns nullptr", [&allocator](){ return allocator.Allocate(32, 1) == nullptr; }());

		{
			// TODO: temp tests, remove
			// success
			printf("allocator.Allocate(0, 1)...\n");
			allocator.Allocate(0, 1);
			printf("allocator.Allocate(0, 2)...\n");
			allocator.Allocate(0, 2);

			// fail
			//printf("allocator.Allocate(0, 0)...\n");
			//allocator.Allocate(0, 0);
			//printf("allocator.Allocate(0, 3)...\n");
			//allocator.Allocate(0, 3);
		}

	}

	// You can do whatever you want here in the main function

	// Here the assignment tests will happen - it will test basic allocator functionality.
	{

	}
}