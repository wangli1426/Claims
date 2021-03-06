/*
 * ExpandableBlockStreamIteratorBase.cpp
 *
 *  Created on: Mar 8, 2014
 *      Author: wangli
 */

#include "ExpandableBlockStreamIteratorBase.h"
#include "../Executor/ExpanderTracker.h"
#include "../utility/CpuScheduler.h"
ExpandableBlockStreamIteratorBase::ExpandableBlockStreamIteratorBase(unsigned number_of_barrier,unsigned number_of_seriliazed_section)
:number_of_barrier_(number_of_barrier),number_of_seriliazed_section_(number_of_seriliazed_section),number_of_registered_expanded_threads_(0){
	barrier_=new Barrier[number_of_barrier_];
	seriliazed_section_entry_key_=new semaphore[number_of_seriliazed_section_];
}

ExpandableBlockStreamIteratorBase::~ExpandableBlockStreamIteratorBase() {
	pthread_mutex_destroy(&sync_lock_);
	pthread_cond_destroy(&sync_cv_);
	for(unsigned i=0;i<number_of_seriliazed_section_;i++){
		seriliazed_section_entry_key_[i].destroy();
	}
	delete[] barrier_;
	delete[] seriliazed_section_entry_key_;
}
void ExpandableBlockStreamIteratorBase::initialize_expanded_status(){
	int ret;
	ret = pthread_mutex_init(&sync_lock_, NULL);
	if(ret!=0)
		printf("pthread_mutex_init failed at barrier creation.\n");
	ret = pthread_cond_init(&sync_cv_, NULL);
	if(ret!=0)
		printf("pthread_cond_init failed at barrier creation.\n");

	for(unsigned i=0;i<number_of_barrier_;i++){
		barrier_[i].setEmpty();
	}

	for(unsigned i=0;i<number_of_seriliazed_section_;i++){
		seriliazed_section_entry_key_[i].set_value(1);
	}
}
bool ExpandableBlockStreamIteratorBase::tryEntryIntoSerializedSection(unsigned phase_id){
	assert(phase_id<number_of_seriliazed_section_);
	return seriliazed_section_entry_key_[phase_id].try_wait();
}
void ExpandableBlockStreamIteratorBase::RegisterExpandedThreadToAllBarriers(){
	lock_number_of_registered_expanded_threads_.acquire();
	number_of_registered_expanded_threads_++;
	lock_number_of_registered_expanded_threads_.release();
	for(unsigned i=0;i<number_of_barrier_;i++){
		barrier_[i].RegisterOneThread();
	}
}

void ExpandableBlockStreamIteratorBase::unregisterExpandedThreadToAllBarriers(unsigned barrier_index){
	number_of_registered_expanded_threads_--;
	lock_number_of_registered_expanded_threads_.release();
	for(unsigned i=barrier_index;i<number_of_barrier_;i++){
		barrier_[i].UnregisterOneThread();
	}
}

void ExpandableBlockStreamIteratorBase::barrierArrive(unsigned barrier_index){
	assert(barrier_index<number_of_barrier_);
	barrier_[barrier_index].Arrive();
}
void ExpandableBlockStreamIteratorBase::destoryAllContext(){
	for(boost::unordered_map<pthread_t,thread_context*>::const_iterator it=context_list_.begin();it!=context_list_.cend();it++){
		delete it->second;
	}
	for(int i=0;i<free_context_list_.size();i++){
		delete free_context_list_[i];
	}
}
//void ExpandableBlockStreamIteratorBase::destorySelfContext(){
//	context_lock_.acquire();
//	/* assert that no context is available for current thread*/
//	assert(context_list_.find(pthread_self())!=context_list_.cend());
//
////	thread_context tc;
////	tc.iterator_=tc.block_for_asking_->createIterator();
////	assert(tc.iterator_->currentTuple()==0);
//	context_list_.erase(pthread_self());
////	printf("Thread %lx is inited!\n",pthread_self());
//	context_lock_.release();
//}
void ExpandableBlockStreamIteratorBase::initContext(thread_context* tc){
	context_lock_.acquire();
	/* assert that no context is available for current thread*/
	assert(context_list_.find(pthread_self())==context_list_.cend());

	context_list_[pthread_self()]=tc;
//	printf("Thread %llx is inited! context:%llx\n",pthread_self(),tc);
	context_lock_.release();
}
thread_context* ExpandableBlockStreamIteratorBase::getContext(){
	thread_context* ret;
	boost::unordered_map<pthread_t,thread_context*>::const_iterator it;
	context_lock_.acquire();
	if((it=context_list_.find(pthread_self()))!=context_list_.cend()){
		ret= it->second;
	}
	else
	{
		ret=0;
	}
//	printf("Thread %lx is poped!\n",pthread_self());
	context_lock_.release();
	return ret;
}

bool ExpandableBlockStreamIteratorBase::checkTerminateRequest() {
	return  ExpanderTracker::getInstance()->isExpandedThreadCallBack(pthread_self());
}

void ExpandableBlockStreamIteratorBase::setReturnStatus(bool ret) {
	ret=open_ret_&&ret;
}

thread_context* ExpandableBlockStreamIteratorBase::createOrReuseContext(
		context_reuse_mode crm) {
	thread_context* target=getFreeContext(crm);
	if(target!=0)
		return target;
	target= createContext();
	target->set_locality_(getCurrentCpuAffility());
	initContext(target);
	return target;
}

bool ExpandableBlockStreamIteratorBase::getReturnStatus() const {
	return open_ret_;
}

thread_context* ExpandableBlockStreamIteratorBase::getFreeContext(
		context_reuse_mode crm) {
	int32_t locality=getCurrentCpuAffility();
	for(int i=0;i<free_context_list_.size();i++){
		switch(crm){
		case crm_no_reuse:
			return 0;
		case crm_core_sensitive:
			if(locality==free_context_list_[i]->get_locality_())
				return free_context_list_[i];
			break;
		case crm_numa_sensitive:
			if(getCurrentSocketAffility()==getSocketAffility(free_context_list_[i]->get_locality_()))
				return free_context_list_[i];
			break;
		case crm_anyway:
			return free_context_list_[i];
		default:
			break;
		}
	}
	return 0;
}
