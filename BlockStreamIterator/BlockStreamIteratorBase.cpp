/*
 * BlockStreamIteratorBase.cpp
 *
 *  Created on: Aug 26, 2013
 *      Author: wangli
 */

#include <iostream>
#include <assert.h>
#include "BlockStreamIteratorBase.h"
#include "ParallelBlockStreamIterator/ExpandableBlockStreamSingleColumnScan.h"

using namespace std;

BlockStreamIteratorBase::BlockStreamIteratorBase():recent_visit_(0),recent_selectivity_(0) {
	// TODO Auto-generated constructor stub

}

BlockStreamIteratorBase::~BlockStreamIteratorBase() {
	// TODO Auto-generated destructor stub
}

BlockStreamIteratorBase* BlockStreamIteratorBase::createIterator(const string &IteratorName){
//	if(IteratorName=="scan"){
//		cout<<"scan iterator"<<endl;
//		return new ExpandableBlockStreamSingleColumnScan();
//	}
}
ResultSet* BlockStreamIteratorBase::getResultSet(){
	printf("You cannot get ResultSet from Root operator!\n");
	assert(false);
	return 0;
}
void BlockStreamIteratorBase::updateRecentVisit(double value) {
	recent_visit_=recent_visit_*0.6+value*0.4;
}

double BlockStreamIteratorBase::getRecentVisit() const {
	return recent_visit_;
}

void BlockStreamIteratorBase::updateSelectivity(double value) {
	recent_selectivity_=recent_selectivity_*0.6+value*0.4;
}

double BlockStreamIteratorBase::getSelectivity() const {
	return recent_selectivity_;
}
