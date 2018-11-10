#ifndef MUTUA_CONTAINERS_NONBLOCKINGNONREENTRANTZEROCOPYQUEUE_H_
#define MUTUA_CONTAINERS_NONBLOCKINGNONREENTRANTZEROCOPYQUEUE_H_

namespace mutua::containers {

    /**
     * NonBlockingNonReentrantPointerQueue.h
     * =====================================
     * created (in C++) by luiz, Nov 2, 2018
     *
     * Simpler and 20% faster than std queue, allowing zero-copy of elements.
     *
    */
	template <typename _QueueElement, typename _QueueSlotsType = uint_fast8_t>
	class NonBlockingNonReentrantZeroCopyQueue {

	private:

		_QueueSlotsType queueHead;			// will never be behind of 'queueReservedHead'
		_QueueSlotsType queueTail;			// will never be  ahead of 'queueReservedTail'
		_QueueSlotsType queueReservedHead;	// will never be  ahead of 'queueHead'
		_QueueSlotsType queueReservedTail;	// will never be behind of 'queueTail'
		bool          reservations[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// keeps track of the conceded but not yet enqueued & conceded but not yet dequeued positions
		_QueueElement backingArray[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// an array sized like this allows implicit modulus operations on indexes of the same type (_QueueSlotsType)

	public:

		NonBlockingNonReentrantZeroCopyQueue()
				: queueHead         (0)
				, queueTail         (0)
				, queueReservedTail (0)
				, queueReservedHead (0)
				, reservations      {false} {}

		// TODO another constructor might receive pointers to all local variables -- allowing for mmapped backed queues

		/** points 'slotPointer' to a reserved queue position, for later enqueueing. When using this 'zero-copy' enqueueing method, do as follows:
		 *    _QueueElement* element;
		 *    if (reserveForEnqueue(element)) {
		 *    	... (fill 'element' with data) ...
		 *    	enqueueReservedSlot(element);
		 *    } */
		inline bool reserveForEnqueue(_QueueElement& slotPointer) {
			if (queueReservedHead-queueReservedTail == 1) {
				if (reservations[queueReservedHead]) {
					return false;
				} else {
					queueReservedHead++;
				}
			}
			reservations[queueReservedTail] = true;
			slotPointer = &backingArray[queueReservedTail];
			queueReservedTail++;
			return true;
		}

		inline void enqueueReservedSlot(const _QueueElement& slotPointer) {
			_QueueSlotsType reservedPos = slotPointer-backingArray;
			reservations[reservedPos] = false;
			if (reservedPos == queueTail) {
				queueTail++;
			}
		}

		inline bool reserveForDequeue(_QueueElement& slotPointer) {
			if (queueHead == queueTail) {
				if (reservations[queueTail]) {
					return false;
				} else {
					queueTail++;
				}
			}
			reservations[queueHead] = true;
			slotPointer = &backingArray[queueHead];
			queueHead++;
			return true;
		}

		inline void dequeueReservedSlot(const _QueueElement& slotPointer) {
			_QueueSlotsType reservedPos = slotPointer-backingArray;
			reservations[reservedPos] = false;
			if (reservedPos == queueReservedHead) {
				queueReservedHead++;
			}
		}
	};
}
#endif /* MUTUA_CONTAINERS_NONBLOCKINGNONREENTRANTZEROCOPYQUEUE_H_ */