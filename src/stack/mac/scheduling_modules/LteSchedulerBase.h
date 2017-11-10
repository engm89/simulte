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
#include "common/LteCommon.h"
#include "common/oracle/Oracle.h"
#include "stack/mac/buffer/LteMacBuffer.h"

/**
 * Derive from this class to get basic functionality.
 */
class LteSchedulerBase : public virtual LteScheduler {
private:
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it. */
	std::map<MacCid, std::vector<Band>> schedulingDecisions;
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it for frequency reuse. */
	std::map<MacCid, std::vector<Band>> reuseDecisions;
	size_t numTTIs = 0, numTTIsWithNoActives = 0;

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
	 * Requests a scheduling grant for the given connection and list of resources.
	 */
	SchedulingResult request(MacCid connectionId, std::vector<Band> resources) {
		bool terminate = false;
		bool active = true;
		bool eligible = true;

		std::vector<BandLimit> bandLimitVec;
		for (const Band& band : resources)
		    bandLimitVec.push_back(BandLimit(band));

		// requestGrant(...) might alter the three bool values, so we can check them afterwards.
		unsigned long max = 4294967295U; // 2^32
		unsigned int granted = requestGrant(connectionId, max, terminate, active, eligible, &bandLimitVec);

        SchedulingResult result;
		if (terminate)
			result = SchedulingResult::TERMINATE;
		else if (!active)
		    result = SchedulingResult::INACTIVE;
		else if (!eligible)
		    result = SchedulingResult::INELIGIBLE;
		else
		    result = SchedulingResult::OK;

		EV << NOW << " LteSchedulerBase::request Scheduled node " << MacCidToNodeId(connectionId) << " on RBs";
        for (const Band& resource : resources)
            EV << " " << resource;
        EV << ": " << schedulingResultToString(result) << std::endl;

        return result;
	}

	/**
	 * Bypasses the allocator, and instead directly allocates resources to nodes.
	 */
	void allocate(std::map<MacCid, std::vector<Band>> allocationMatrix) {
		// This list'll be filled out.
		std::vector<std::vector<AllocatedRbsPerBandMapA>> allocatedRbsPerBand;
		allocatedRbsPerBand.resize(MAIN_PLANE + 1); // Just the main plane.
		allocatedRbsPerBand.at(MAIN_PLANE).resize(MACRO + 1); // Just the MACRO antenna.
		for (const auto& allocationItem : allocationMatrix) {
			const MacCid connectionId = allocationItem.first;
			const MacNodeId nodeId = MacCidToNodeId(connectionId);
			const std::vector<Band>& resources = allocationItem.second;
			// For every resource...
			for (const Band& resource : resources) {
				// ... determine the number of bytes we can serve ...
				Codeword codeword = 0;
				unsigned int numRBs = 1;
				Direction dir;
				if (MacCidToLcid(connectionId) == D2D_MULTI_SHORT_BSR)
					dir = D2D_MULTI;
				else
					dir = (MacCidToLcid(connectionId) == D2D_SHORT_BSR) ? D2D : direction_;
				unsigned int numBytesServed = eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, resource, codeword, numRBs, dir);
				// ... and store the decision in this data structure.
				allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedRbsMap_[nodeId] += 1;
				allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedBytesMap_[nodeId] += numBytesServed;
				allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].allocated_ += 1;
			}

			// Add the nodeId to the scheduleList - needed for grants.
			std::pair<unsigned int, Codeword> scheduleListEntry;
			scheduleListEntry.first = connectionId;
			scheduleListEntry.second = 0; // Codeword.
			eNbScheduler_->storeScListId(scheduleListEntry, resources.size());

			EV << NOW << " LteSchedulerBase::requestReuse Scheduled node " << MacCidToNodeId(connectionId)
			   << " for frequency reuse on RBs";
			for (const Band& resource : resources)
				EV << " " << resource;
			EV << std::endl;
		}
		// Tell the scheduler about our decision.
//		if (*Oracle::get()->printAllocation(allocatedRbsPerBand);
		eNbScheduler_->storeAllocationEnb(allocatedRbsPerBand, NULL);
	}

//	/**
//	 * Lets the given connection reuse the given resources by bypassing the allocator.
//	 */
//	void requestReuse(MacCid connectionId, std::vector<Band> resources) {
//		// This list'll be filled out.
//		std::vector<std::vector<AllocatedRbsPerBandMapA>> allocatedRbsPerBand;
//		allocatedRbsPerBand.resize(MAIN_PLANE + 1); // Just the main plane.
//		allocatedRbsPerBand.at(MAIN_PLANE).resize(MACRO + 1); // Just the MACRO antenna.
//		MacNodeId nodeId = MacCidToNodeId(connectionId);
//		// For every resource...
//		for (const Band& resource : resources) {
//			// ... determine the number of bytes we can serve ...
//			Codeword codeword = 0;
//			unsigned int numRBs = 1;
//			Direction dir;
//			if (MacCidToLcid(connectionId) == D2D_MULTI_SHORT_BSR)
//				dir = D2D_MULTI;
//			else
//				dir = (MacCidToLcid(connectionId) == D2D_SHORT_BSR) ? D2D : direction_;
//			unsigned int numBytesServed = eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, resource, codeword, numRBs, dir);
//			// ... and store the decision in this data structure.
//			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedRbsMap_[nodeId] += 1;
//			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedBytesMap_[nodeId] += numBytesServed;
//			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].allocated_ += 1;
//		}
//		// Tell the scheduler about our decision.
//		std::set<Band> occupiedBands = eNbScheduler_->getOccupiedBands();
//		eNbScheduler_->storeAllocationEnb(allocatedRbsPerBand, NULL);
//		// Add the nodeId to the scheduleList - needed for grants.
//		std::pair<unsigned int, Codeword> scheduleListEntry;
//		scheduleListEntry.first = connectionId;
//		scheduleListEntry.second = 0; // Codeword.
//		eNbScheduler_->storeScListId(scheduleListEntry, resources.size());
//
//		EV << NOW << " LteSchedulerBase::requestReuse Scheduled node " << MacCidToNodeId(connectionId)
//				<< " for frequency reuse on RBs";
//		for (const Band& resource : resources)
//			EV << " " << resource;
//		EV << std::endl;
//	}

protected:
	void scheduleUe(const MacCid& connection, const Band& resource) {
		schedulingDecisions[connection].push_back(resource);
	}

	void scheduleUeReuse(const MacCid& connection, const Band& resource) {
		reuseDecisions[connection].push_back(resource);
	}

public:
	LteSchedulerBase() {}
	virtual ~LteSchedulerBase() {
		std::cout << dirToA(direction_) << ": " << numTTIs << " TTIs, of which "
				<< numTTIsWithNoActives << " had no active connections: numNoActives/numTTI="
				<< ((double) numTTIsWithNoActives / (double) numTTIs) << std::endl;
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
	 * Schedule all connections and save decisions to the schedulingDecisions map.
	 */
	virtual void schedule(std::set<MacCid>& connections) = 0;

	virtual void prepareSchedule() override {
		EV << NOW << " LteSchedulerBase::prepareSchedule" << std::endl;
		// Copy currently active connections to a working copy.
		activeConnectionTempSet_ = activeConnectionSet_;
		// Prepare this round's scheduling decision map.
		schedulingDecisions.clear();
		reuseDecisions.clear();
		// Keep track of the number of TTIs.
		numTTIs++;
		if (activeConnectionTempSet_.size() == 0) {
			numTTIsWithNoActives++;
			return;
		} else {
			// Make sure each connection belongs to an existing node in the simulation.
			for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end();) {
				const MacCid& connection = *iterator;
				MacNodeId id = MacCidToNodeId(connection);

				// Make sure it's not been dynamically removed.
				if (id == 0 || getBinder()->getOmnetId(id) == 0) {
					EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
							<< id << " removed from active connection set - appears to have been dynamically removed."
							<< std::endl;
					iterator = activeConnectionTempSet_.erase(iterator);
					continue;
				}

				// Make sure it's active.
				if (eNbScheduler_->bsrbuf_->at(connection)->isEmpty()) {
					EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
							<< id << " removed from active connection set - no longer active as BSR buffer is empty."
							<< std::endl;
					iterator = activeConnectionTempSet_.erase(iterator);
					continue;
				}
				// Make sure CQI>0.
				Direction dir;
				if (direction_ == UL)
					dir = (MacCidToLcid(connection) == D2D_SHORT_BSR) ? D2D : (MacCidToLcid(connection) == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : direction_;
				else
					dir = DL;
				const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(id, dir);
				const std::set<Band>& bands = info.readBands();
				unsigned int codewords = info.getLayers().size();
				bool cqiNull = false;
				for (unsigned int i=0; i < codewords; i++) {
					if (info.readCqiVector()[i] == 0)
						cqiNull=true;
				}
				if (cqiNull) {
					EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
							<< id << " removed from active connection set - CQI is zero."
							<< std::endl;
					iterator = activeConnectionTempSet_.erase(iterator);
					continue;
				}
				iterator++; // Only increment if we haven't erased an item. Otherwise the erase returns the next valid position.
			}
		}

		// Check again, might have deleted all connections.
		if (activeConnectionTempSet_.size() == 0) {
			numTTIsWithNoActives++;
			return;
		}

		// All connections that still remain can actually be scheduled.
		for (const MacCid& connection : activeConnectionTempSet_) {
			// Instantiate a resource list for each connection.
			schedulingDecisions[connection] = std::vector<Band>();
			reuseDecisions[connection] = std::vector<Band>();
		}
		// Call respective schedule() function implementation.
		schedule(activeConnectionTempSet_);
	}

	virtual void commitSchedule() override {
		EV << NOW << " LteSchedulerBase::commitSchedule" << std::endl;
		activeConnectionSet_ = activeConnectionTempSet_;
		// Request grants for resource allocation.
		for (const auto& item : schedulingDecisions) {
			MacCid connection = item.first;
			if (MacCidToNodeId(connection) == 0)
				continue;
			std::vector<Band> resources = item.second;
			if (resources.size() > 0)
				request(connection, resources);
		}
		// Bypass the allocator and set resources to be frequency-reused.
		for (std::map<MacCid, std::vector<Band>>::iterator it = reuseDecisions.begin(); it != reuseDecisions.end(); it++) {
			if (MacCidToNodeId((*it).first) == 0) {
				reuseDecisions.erase(it);
				it--;
			}
		}
		allocate(reuseDecisions);
//		for (const auto& item : reuseDecisions) {
//			MacCid connection = item.first;
//			if (MacCidToNodeId(connection) == 0)
//				continue;
//			std::vector<Band> resources = item.second;
//			if (resources.size() > 0)
//				requestReuse(connection, resources);
//		}
	}

	const size_t getNumTTIs() const {
		return numTTIs;
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_ */
