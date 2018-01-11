/*
 * LteSchedulerBase.h
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_
#define STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_

#include <omnetpp.h>
#include "stack/mac/scheduler/LteScheduler.h"
#include "stack/mac/scheduler/LteSchedulerEnb.h"
#include "common/LteCommon.h"
#include "common/oracle/Oracle.h"
#include "stack/mac/buffer/LteMacBuffer.h"

/**
 * Derive from this class to get basic functionality.
 */
class LteSchedulerBase : public virtual LteScheduler {
public:
	LteSchedulerBase() {}
	virtual ~LteSchedulerBase();

	enum SchedulingResult {
		OK = 0, TERMINATE, INACTIVE, INELIGIBLE
	};

	std::string schedulingResultToString(SchedulingResult result) {
		return (result == SchedulingResult::TERMINATE ? "TERMINATE" :
				result == SchedulingResult::INACTIVE ? "INACTIVE" :
				result == SchedulingResult::INELIGIBLE ? "INELIGIBLE"
				: "OK");
	}

	/**
	 * When the LteSchedulerEnb learns of an active connection it notifies the LteScheduler.
	 * It is essential to save this information. (I think this should be the default behaviour and be done in the LteScheduler class)
	*/
	void notifyActiveConnection(MacCid cid) override {
		EV_STATICCONTEXT;
		EV << NOW << " LteSchedulerBase::notifyActiveConnection(" << cid << ")" << std::endl;
		activeConnectionSet_.insert(cid);
	}

	/**
	 * When the LteSchedulerEnb learns of a connection going inactive it notifies the LteScheduler.
	*/
	void removeActiveConnection(MacCid cid) override {
		EV_STATICCONTEXT;
		EV << NOW << " LteSchedulerBase::removeActiveConnection(" << cid << ")" << std::endl;
		activeConnectionSet_.erase(cid);
	}

	/**
	 * Schedule all connections. Call scheduleUe and scheduleUeReuse to remember decisions.
	 * Decisions will be applied at commitSchedule().
	 */
	virtual void schedule(std::set<MacCid>& connections) = 0;

	virtual void prepareSchedule() override;

	/**
	 * When a schedule is committed, this function can react to scheduling grant request results in whatever way.
	 */
	virtual void reactToSchedulingResult(const SchedulingResult& result, unsigned int numBytesGranted, const MacCid& connection) {
		// Override to do something here.
	}

	virtual void commitSchedule() override;

	const size_t getNumTTIs() const {
		return numTTIs;
	}

protected:
	unsigned int numBytesGrantedLast = 0;

	/**
	 * Remember to schedule 'resource' to 'connection' at the end of this scheduling round.
	 */
	void scheduleUe(const MacCid& connection, const Band& resource) {
		schedulingDecisions[connection].push_back(resource);
	}

	/**
	 * Remember to schedule 'resource' to 'connection' at the end of this scheduling round by bypassing the allocator and directly setting
	 * the corresponding entry in the allocation matrix.
	 */
	void scheduleUeReuse(const MacCid& connection, const Band& resource) {
		reuseDecisions[connection].push_back(resource);
	}

private:
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it. */
	std::map<MacCid, std::vector<Band>> schedulingDecisions;
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it for frequency reuse. */
	std::map<MacCid, std::vector<Band>> reuseDecisions;
	size_t numTTIs = 0, numTTIsWithNoActives = 0;

	/**
	 * Requests a scheduling grant for the given connection and list of resources.
	 */
	SchedulingResult request(const MacCid& connectionId, const std::vector<Band>& resources);

	/**
	 * Bypasses the allocator, and instead directly allocates resources to nodes.
	 */
	void allocate(std::map<MacCid, std::vector<Band>> allocationMatrix);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_ */
