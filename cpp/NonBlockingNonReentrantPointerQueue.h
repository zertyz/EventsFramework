#ifndef MUTUA_CONTAINERS_NONBLOCKINGNONREENTRANTPOINTERQUEUE_H_
#define MUTUA_CONTAINERS_NONBLOCKINGNONREENTRANTPOINTERQUEUE_H_

namespace mutua::containers {

    /**
     * NonBlockingNonReentrantPointerQueue.h
     * =====================================
     * created (in C++) by luiz, Nov 3, 2018
     *
     * Simpler and 20% faster than std queue.
     *
    */
	template <typename _QueueElement, typename _QueueSlotsType = uint_fast8_t>
	class NonBlockingNonReentrantPointerQueue {
	
	private:

		_QueueSlotsType queueHead;
		_QueueSlotsType queueTail;
		_QueueElement* backingArray[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// an array sized like this allows implicit modulus operations on indexes of the same type (_QueueSlotsType)

	public:

		NonBlockingNonReentrantPointerQueue()
				: queueHead(0)
				, queueTail(0) {}

		// TODO another constructor might receive pointers to all local variables -- allowing for mmapped backed queues

		inline bool enqueue(_QueueElement& elementToEnqueue) {
			if (queueHead-queueTail == 1) {
				return false;
			}
			backingArray[queueTail++] = &elementToEnqueue;
			return true;
		}

		inline bool dequeue(_QueueElement*& dequeuedElementPointer) {
			if (queueHead == queueTail) {
				return false;
			}
			dequeuedElementPointer = backingArray[queueHead++];
			return true;
		}
	};
}
#endif /* MUTUA_CONTAINERS_NONBLOCKINGNONREENTRANTPOINTERQUEUE_H_ */