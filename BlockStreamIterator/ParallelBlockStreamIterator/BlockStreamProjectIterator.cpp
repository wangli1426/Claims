/*
 * BlockStreamProjectIterator.cpp
 *
 *  Created on: 2014-2-17
 *      Author: casa
 */

#include "BlockStreamProjectIterator.h"

BlockStreamProjectIterator::BlockStreamProjectIterator() {
	initialize_expanded_status();
}

BlockStreamProjectIterator::~BlockStreamProjectIterator() {

}

BlockStreamProjectIterator::BlockStreamProjectIterator(State state)
:state_(state){
	initialize_expanded_status();
}

BlockStreamProjectIterator::State::State(Schema * input, Schema* output, BlockStreamIteratorBase * children, unsigned blocksize, Mapping map, vector<ExpressItem_List> v_ei,vector<QNode *>exprTree)
:input_(input),output_(output),child_(children),block_size_(blocksize),map_(map),v_ei_(v_ei),exprTree_(exprTree){

}
BlockStreamProjectIterator::State::State(Schema * input, Schema* output, BlockStreamIteratorBase * children, unsigned blocksize, Mapping map, vector<ExpressItem_List> v_ei)
:input_(input),output_(output),child_(children),block_size_(blocksize),map_(map),v_ei_(v_ei){

}
bool BlockStreamProjectIterator::open(const PartitionOffset& partition_offset){
	project_thread_context* ptc =(project_thread_context*)createOrReuseContext(crm_core_sensitive);

	bool ret=state_.child_->open(partition_offset);
	setReturnStatus(ret);
	barrierArrive();
	return getReturnStatus();
}

/*
 * now the expressions computing speed is slow because of the copy between among the expressions
 * todo: seek the pointer of data and LLVM will be solved by wangli.
 * */
bool BlockStreamProjectIterator::next(BlockStreamBase *block){
	unsigned total_length_=state_.output_->getTupleMaxSize();

	void* tuple_from_child;
	void* tuple_in_block;
	project_thread_context* tc = (project_thread_context*) getContext();
	while(true){
		if(tc->block_stream_iterator_->currentTuple()==0){
			/* mark the block as processed by setting it empty*/
			tc->block_for_asking_->setEmpty();
			if(state_.child_->next(tc->block_for_asking_)){
//				printf("%lld\n",pthread_self());

				delete tc->block_stream_iterator_;
				tc->block_stream_iterator_=tc->block_for_asking_->createIterator();
				updateRecentVisit(tc->block_for_asking_->getVisit());
				tc->tuples_read_+=tc->block_for_asking_->getTuplesInBlock();
			}
			else
			{
				if (!block->Empty()) {
					long tuple_consumed=tc->tuples_read_+(tc->block_stream_iterator_->get_cur()-tc->start_cur_);
					int tuple_produced=block->getTuplesInBlock();
					double instant_selectivity=(double)tuple_produced/tuple_consumed;
					tc->tuples_read_=0;
					tc->start_cur_=tc->block_stream_iterator_->get_cur();
					updateSelectivity(instant_selectivity);
					double selectivity=getSelectivity();
					double new_visit=getRecentVisit()*selectivity;
					block->setVisit(new_visit);
					return true;
				} else {
					return false;
				}
			}
		}
		process_logic(block,tc);
		/* there are totally two reasons for the end of the while loop.
		 * (1) block is full of tuples satisfying filter (should return true to the caller)
		 * (2) block_for_asking_ is exhausted (should fetch a new block from child and continue to process)
		 */
		if(block->Full()){
			// for case (1)
			long tuple_consumed=tc->tuples_read_+(tc->block_stream_iterator_->get_cur()-tc->start_cur_);
			int tuple_produced=block->getTuplesInBlock();
			double instant_selectivity=(double)tuple_produced/tuple_consumed;
			tc->tuples_read_=0;
			tc->start_cur_=tc->block_stream_iterator_->get_cur();
			updateSelectivity(instant_selectivity);
			double selectivity=getSelectivity();
			double new_visit=getRecentVisit()*selectivity;
			block->setVisit(new_visit);
			return true;
		}
		else{
		}
	}

}

bool BlockStreamProjectIterator::close(){
	initialize_expanded_status();
	destoryAllContext();
	return state_.child_->close();
}


bool BlockStreamProjectIterator::copyNewValue(void *tuple,void *result,int length){
	memcpy(tuple,result,length);
}
bool BlockStreamProjectIterator::copyColumn(void *&tuple,ExpressionItem &result,int length){
	switch(result.return_type){
		case t_int:{
			memcpy(tuple,&result.content.data.value._int,length);
			break;
		}
		case t_float:{
			memcpy(tuple,&result.content.data.value._float,length);
			break;
		}
		case t_u_long:{
			memcpy(tuple,&result.content.data.value._ulong,length);
			break;
		}
		case t_string:{
			memcpy(tuple,result._string.c_str(),length);
			break;
		}
		case t_decimal:{
			memcpy(tuple,&result._decimal,length);
			break;
		}
		case t_date:{
			memcpy(tuple,&result._date,length);
			break;
		}
		case t_time:{
			memcpy(tuple,&result._time,length);
			break;
		}
		case t_datetime:{
			memcpy(tuple,&result._datetime,length);
			break;
		}
		case t_double:{
			memcpy(tuple,&result.content.data.value._double,length);
			break;
		}
		case t_smallInt:{
			memcpy(tuple,&result.content.data.value._sint,length);
			break;
		}
		default:{
			cout<<"missing the operator!!!"<<endl;
			break;
		}
	}
}
void BlockStreamProjectIterator::print()
{
	cout<<"proj:"<<endl;
	for(int i=0;i<state_.exprTree_.size();i++)
	{
		printf("	%s\n",state_.exprTree_[i]->alias.c_str());
	}
	state_.child_->print();

}

void BlockStreamProjectIterator::process_logic(BlockStreamBase* block,
		project_thread_context* tc) {
	unsigned total_length=state_.output_->getTupleMaxSize();
	void* tuple_from_child;
	void* tuple;
	while ((tuple_from_child = tc->block_stream_iterator_->currentTuple()) > 0) {
		if((tuple=block->allocateTuple(total_length))>0)
		{
			for(int i=0;i<tc->thread_qual_.size();i++)
			{
				void * result=tc->thread_qual_[i]->FuncId(tc->thread_qual_[i],tuple_from_child,state_.input_);
				copyNewValue(tuple,result,state_.output_->getcolumn(i).get_length());
				tuple=(char *)tuple+state_.output_->getcolumn(i).get_length();
			}
			tc->block_stream_iterator_->increase_cur_();
		}
		else{
			return;
		}
	}
	/* mark the block as processed by setting it empty*/
	tc->block_for_asking_->setEmpty();
}

thread_context* BlockStreamProjectIterator::createContext() {
	project_thread_context* ptc = new project_thread_context();
	ptc->block_for_asking_=BlockStreamBase::createBlock(state_.input_,state_.block_size_);
	ptc->temp_block_ = BlockStreamBase::createBlock(state_.output_,state_.block_size_);
	ptc->block_stream_iterator_ = ptc->block_for_asking_->createIterator();
	ptc->thread_qual_=state_.exprTree_;
	for(int i=0;i<state_.exprTree_.size();i++){
		Expr_copy(state_.exprTree_[i],ptc->thread_qual_[i]);
		InitExprAtPhysicalPlan(ptc->thread_qual_[i]);
	}
	ptc->start_cur_=0;
	ptc->tuples_read_=0;
	return ptc;
}
