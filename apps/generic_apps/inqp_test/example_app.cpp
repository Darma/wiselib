


/// ----- Config


#if defined(SHAWN)
	#define WISELIB_MAX_NEIGHBORS 10000
	#define WISELIB_TIME_FACTOR 1

#elif defined(PC)
	#define WISELIB_TIME_FACTOR 1
	#define WISELIB_DISABLE_DEBUG 0
	#define WISELIB_DISABLE_DEBUG_MESSAGES 0

#else
	#define WISELIB_MAX_NEIGHBORS 4

#endif
#define INQP_TEST_USE_BLOCK 0
#define INQP_AGGREGATE_CHECK_INTERVAL 1000
#define WISELIB_MAX_NEIGHBORS 100


/// ------ /Config



#if ISENSE
	extern "C" void assert(int) { }
#endif

#include <external_interface/external_interface.h>
#include <external_interface/external_interface_testing.h>
#ifdef ISENSE
	void* malloc(size_t n) { return isense::malloc(n); }
	void free(void* p) { isense::free(p); }
#endif
	
#if defined(ARDUINO)
	#include <external_interface/arduino/arduino_monitor.h>
#elif defined(ISENSE)
	#include <external_interface/isense/isense_monitor.h>
#endif

#ifndef DBG
	#define DBG(X)
	#define DBG(X, ...)
#endif
	
// Is there any Os for which we really want this at all?
#if !defined(ISENSE) && !defined(PC) && !defined(ARDUINO) && !defined(SHAWN)
	void assert(int) { }
#endif
	
typedef wiselib::OSMODEL Os;
typedef Os::block_data_t block_data_t;
using namespace wiselib;

#include <util/allocators/malloc_free_allocator.h>
typedef MallocFreeAllocator<Os> Allocator;
Allocator& get_allocator();

#include <util//string_util.h>
//#include <algorithms/rdf/inqp/query.h>
#include <algorithms/rdf/inqp/query_processor.h>
//#include <algorithms/rdf/inqp/table.h>
//#include <algorithms/rdf/inqp/row.h>
#include <algorithms/rdf/inqp/communicator.h>
#include <util/meta.h>
#include <util/debugging.h>
#include <util/pstl/map_static_vector.h>
#include <util/pstl/priority_queue_dynamic.h>
#include <util/tuple_store/tuplestore.h>
#include "tuple.h"

#include <algorithms/routing/flooding_nd/flooding_nd.h>
#include <algorithms/protocols/packing_radio/packing_radio.h>
//#include <algorithms/routing/tree_routing_ndis/tree_routing_ndis.h>
#include <algorithms/routing/forward_on_directed_nd/forward_on_directed_nd.h>
#include <algorithms/hash/sdbm.h>
typedef wiselib::Sdbm<Os> Hash;

typedef wiselib::FloodingNd<Os, Os::Radio> FNDRadio;
typedef wiselib::PackingRadio<Os, FNDRadio> PRadio;

//typedef wiselib::TreeRoutingNdis<Os, Os::Radio, Os::Clock, Os::Timer, FNDRadio, Os::Debug> TRadio;
typedef wiselib::ForwardOnDirectedNd<Os, Os::Radio, FNDRadio> TRadio;
typedef wiselib::PackingRadio<Os, TRadio> PAnsRadio;

// -------- BEGIN TS SETUP

typedef Tuple<Os> TupleT;


// RAM
#if !INQP_TEST_USE_BLOCK
//#include <util/pstl/list_dynamic.h>
#include <util/pstl/unique_container.h>
#include <util/tuple_store/prescilla_dictionary.h>
#include <util/pstl/vector_static.h>
//#include <util/pstl/unbalanced_tree_dictionary.h>
//typedef wiselib::list_dynamic<Os, TupleT> TupleList;
//typedef wiselib::UniqueContainer<TupleList> TupleContainer;
typedef wiselib::vector_static<Os, TupleT, 100> TupleContainer;
typedef wiselib::PrescillaDictionary<Os> Dictionary;
//typedef UnbalancedTreeDictionary<Os> Dictionary;
#else

// BLOCK
#define BLOCK_CACHE_SIZE 2
#define BLOCK_CACHE_SPECIAL 1
#define BLOCK_CACHE_WRITE_THROUGH 1
#define BLOCK_CHUNK_SIZE 8
#define BLOCK_CHUNK_ADDRESS_TYPE ::uint32_t

#include <algorithms/block_memory/bitmap_chunk_allocator.h>
#include <algorithms/block_memory/cached_block_memory.h>
typedef CachedBlockMemory<Os, Os::BlockMemory, BLOCK_CACHE_SIZE, BLOCK_CACHE_SPECIAL, BLOCK_CACHE_WRITE_THROUGH> BlockMemory;
typedef BitmapChunkAllocator<Os, BlockMemory, BLOCK_CHUNK_SIZE, BLOCK_CHUNK_ADDRESS_TYPE> BlockAllocator;
#include <algorithms/block_memory/b_plus_hash_set.h>
typedef BPlusHashSet<Os, BlockAllocator, Hash, TupleT, true> TupleContainer;
#include <algorithms/block_memory/b_plus_dictionary.h>
typedef BPlusDictionary<Os, BlockAllocator, Hash> Dictionary;
#endif

typedef wiselib::TupleStore<Os, TupleContainer, Dictionary, Os::Debug, BIN(111), &TupleT::compare> TS;

// -------- END TS SETUP


typedef INQPQueryProcessor<Os, TS, Hash> Processor;
typedef INQPCommunicator<Os, Processor> Communicator;

#define LEFT 0
#define RIGHT 0x80
#define AGAIN 0x80

#define LEFT_COL(X) ((X) << 4)
#define RIGHT_COL(X) ((X) & 0x0f)

#ifdef ISENSE
#include <isense/util/get_os.h>
#endif

#define SINK 1



template<typename OsModel_P>
class NullMonitor {
	public:
		void init(typename OsModel_P::Debug* d) { debug_ = d; }
		
		void report() { }
		void report(const char *remark) { debug_->debug(remark); }
		int free() { return 666; }
		
		typename OsModel_P::Debug* debug_;
};

static const char* tuples[][3] = {
	
#if defined(SHAWN)
	#include "ssp.cpp"
#elif defined(PC)
	#include "nqxe_test.cpp"
#else
	#include "incontextsensing.cpp"
#endif
	{ 0, 0, 0 }
};

class ExampleApplication
{
	public:
		
		template<typename TS>
		void ins(TS& ts, const char* s, const char* p, const char* o) {
			TupleT t;
			t.set(0, (block_data_t*)const_cast<char*>(s));
			t.set(1, (block_data_t*)const_cast<char*>(p));
			t.set(2, (block_data_t*)const_cast<char*>(o));
			ts.insert(t);
		}
		
		void init( Os::AppMainParameter& value )
		{
			radio_ = &wiselib::FacetProvider<Os, Os::Radio>::get_facet( value );
			timer_ = &wiselib::FacetProvider<Os, Os::Timer>::get_facet( value );
			debug_ = &wiselib::FacetProvider<Os, Os::Debug>::get_facet( value );
			clock_ = &wiselib::FacetProvider<Os, Os::Clock>::get_facet( value );
			
			//block_memory_ = &wiselib::FacetProvider<Os, Os::BlockMemory>::get_facet( value );
			//
			
			monitor_.init(debug_);
			
			//test_heap();
			//test_atol();
			//hashes();
			//return;
			
		#if !defined(PC)
			radio_->enable_radio();
			
			// query direction: packing radio over flooding
			
			fndradio_.init(radio_);
			query_radio_.init(fndradio_, *debug_);
			query_radio_.enable_radio();
			
			// answer direction: packing radio over treerouting
			
			//tradio_.init(*radio_, *clock_, *timer_, fndradio_, *debug_);
			tradio_.init(radio_, &fndradio_);
			result_radio_.init(tradio_, *debug_, *timer_);
			result_radio_.enable_radio();
			
			debug_->debug("boot %d", (int)radio_->id());
			
		#endif
			
			//debug_->debug( "Hello World from Example Application! my id=%d app=%p\n", radio_->id(), this );
			
			//print_memstat();
			
			
			init_ts();
			
			//monitor_.report();
			
		#if defined(ISENSE) || defined(PC)
			be_standalone();
			
		#elif defined(SHAWN)
			if(radio_->id() == SINK) {
				debug_->debug("sink\n");
				//be_sink();
				timer_->set_timer<ExampleApplication, &ExampleApplication::be_sink>(100, this, 0);
				
				//timer_->set_timer<ExampleApplication, &ExampleApplication::sink_ask_hash_resolve>(1000, this, 0);
			}
			else {
				be();
			}
		#endif
		}
		
		Dictionary dictionary;
		TupleContainer container;
		TS ts;
		
		void test_atol() {
			DBG("%ld", wiselib::atol((char*)"12345"));
			DBG("%f", wiselib::atof((char*)"12345.67"));
		}
		
		void test_heap() {
			typedef wiselib::PriorityQueueDynamic<Os, int> Heap;
			Heap h;
			
			/*
			h.push(77);
			h.push(2);
			h.push(-4);
			h.push(312);
			h.push(234);
			h.push(456);
			h.push(1);
			h.push(77);
			h.push(3);
			*/
			
			h.push(6);
			h.push(5);
			h.push(4);
			h.push(3);
			h.push(2);
			h.push(1);
			h.push(0);
			
			while(h.size()) {
				debug_->debug("%d", h.pop());
			}
		}
		
		void hashes() {
			const char *strings[] = {
				"A", "measures", "m1", "m2", "has_value", "12", "14"
			};
			for(const char **s = strings; *s; s++) {
				DBG("%-60s %08lx", *s, (unsigned long)Hash::hash((block_data_t*)*s, strlen(*s)));
			}
		}
			
		
		void init_ts() {
		#if !INQP_TEST_USE_BLOCK
			dictionary.init(debug_);
			ts.init(&dictionary, &container, debug_);
		#else
			block_memory_.physical().init();
			delay(1000);
			block_memory_.init();
			block_allocator_.init(&block_memory_, debug_);
			container.init(&block_allocator_, debug_);
			dictionary.init(&block_allocator_, debug_);
		#endif
			
			//debug_->debug("tsi don");
		}
		
		void insert_tuples() {
			int i = 0;
			for( ; tuples[i][0]; i++) {
				//monitor_.report("ins");
				
				//for(int j = 0; j < 3; j++) {
					//debug_->debug("%-60s %08lx", tuples[i][j], (unsigned long)Hash::hash((block_data_t*)tuples[i][j], strlen(tuples[i][j])));
				//}
				
				ins(ts, tuples[i][0], tuples[i][1], tuples[i][2]);
			}
			debug_->debug("ins done: %d tuples", (int)i);
			
		}
		
		/**
		 * @param q { len, 'O' or 'Q', ... }
		 */
		void process_nqxe_message(char* q) {
			assert(q[1] == 'O' || q[1] == 'Q');
			size_t l = q[0];
			process(l, (block_data_t*)(q + 1));
		}
		
		/**
		 * @param op { 'O', qid, oid, .... } or { 'Q', qid, ops }
		 */
		void process(int sz, block_data_t* op) {
			//ian_.handle_operator(op100, 0, sizeof(op100));
			//communicator_.on_receive_query(radio_->id(), sz, op);
			if(op[0] == 'O') {
				ian_.handle_operator(op[1], sz - 2, op + 2);
			}
			else if(op[0] == 'Q') {
				ian_.handle_query_info(op[1], op[2]);
			}
		}
		
		void be_standalone() {
			insert_tuples();
			
			ian_.init(&ts, timer_);
			
	#if defined(PC)
			timer_->set_timer<ExampleApplication, &ExampleApplication::query_nqxe_test>(1000, this, 0);
			
	#else
			communicator_.init(ian_, query_radio_, result_radio_, fndradio_, *timer_);
			communicator_.set_sink(radio_->id());
			// we play in-network node
			fndradio_.set_parent(SINK);
			
			result_radio_.reg_recv_callback<ExampleApplication, &ExampleApplication::sink_receive_answer>( this );
			
			
			//timer_->set_timer<ExampleApplication, &ExampleApplication::query_cross_p>(1000, this, 0);
			//timer_->set_timer<ExampleApplication, &ExampleApplication::query_temp_p>(1000, this, 0);
			timer_->set_timer<ExampleApplication, &ExampleApplication::query_all_p>(1000, this, 0);
	#endif
		}
		
		
		void query_temp_gps1(void*) {
			enum { Q = Communicator::MESSAGE_ID_QUERY, OP = Communicator::MESSAGE_ID_OPERATOR };
			enum { ROOT = 0 };
			enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
			block_data_t qid = 1;
			
			//monitor_.report("bef");
			
			//block_data_t op100[] = { OP, qid, 100, 'a', ROOT, BIN(010101), BIN(0), BIN(0), BIN(0), 3, MIN | AGAIN, AVG | AGAIN, MAX };
			block_data_t op100[] = { OP, qid, 100, 'c', ROOT, BIN(01), BIN(0), BIN(0), BIN(0) };
			
			process(sizeof(op100), op100);
			
			block_data_t op70[]  = { OP, qid,  70, 'g', LEFT | 100, BIN(11), BIN(0), BIN(0), BIN(0), BIN(110), 0xbf, 0x26, 0xb8, 0x2e, 0xb2, 0x38, 0x60, 0xb3 };
			process(sizeof(op70), op70);
			
			block_data_t cmd[]   = { Q, qid, 2 };
			process(sizeof(cmd), cmd);
			
			//monitor_.report("er");
			ian_.erase_query(qid);
			
			//monitor_.report("aft");
			timer_->set_timer<ExampleApplication, &ExampleApplication::query_temp_gps1>(1000, this, 0);
		}
		
		void query_temp_p1() { query_temp_p(0); }
		void query_temp_p(void*) {
			ian_.set_exec_done_callback(Processor::exec_done_callback_t::from_method<ExampleApplication, &ExampleApplication::query_temp_p1>(this));
			timer_->set_timer<ExampleApplication, &ExampleApplication::query_temp>(1000, this, 0);
		}
		void query_temp(void*) {
			enum { Q = Communicator::MESSAGE_ID_QUERY, OP = Communicator::MESSAGE_ID_OPERATOR };
			enum { ROOT = 0 };
			enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
			block_data_t qid = 1;
			
			monitor_.report("bef");
			
			//block_data_t op100[] = { OP, qid, 100, 'a', ROOT, BIN(010101), BIN(0), BIN(0), BIN(0), 3, MIN | AGAIN, AVG | AGAIN, MAX };
			block_data_t op100[] = { OP, qid, 100, 'c', ROOT, BIN(01), BIN(0), BIN(0), BIN(0) };
			
			process(sizeof(op100), op100);
			
			block_data_t op90[]  = { OP, qid,  90, 'j', LEFT | 100, BIN(010000), BIN(0), BIN(0), BIN(0), LEFT_COL(0) | RIGHT_COL(0) };
			process(sizeof(op90), op90);
			
			block_data_t op80[]  = { OP, qid,  80, 'g', RIGHT | 90, BIN(010011), BIN(0), BIN(0), BIN(0), BIN(010), 0x4d, 0x0f, 0x60, 0xb4 };
			process(sizeof(op80), op80);
			
			block_data_t op70[]  = { OP, qid,  70, 'g', LEFT | 90, BIN(11), BIN(0), BIN(0), BIN(0), BIN(110), 0xbf, 0x26, 0xb8, 0x2e, 0xb2, 0x38, 0x60, 0xb3 };
			process(sizeof(op70), op70);
			
			block_data_t cmd[]   = { Q, qid, 4 };
			process(sizeof(cmd), cmd);
			
			monitor_.report("er");
			ian_.erase_query(qid);
			
			monitor_.report("aft");
			//timer_->set_timer<ExampleApplication, &ExampleApplication::query_temp>(1000, this, 0);
		}
		
		void query_nqxe_test(void*) {
			// 1,1,'g',LEFT | 3,BIN(00000011),0,0,0,BIN(110),"<http://purl.oclc.org/NET/ssnx/ssn#observedProperty>","<http://me.exmpl/Temperature>",
			char op0[] = {18, 79, 1, 1, 103, 3, 3, 0, 0, 0, 6, -65, 38, -72, 46, 87, -12, 33, 99};
			process_nqxe_message(op0);
			// 1,2,'g',RIGHT | 3,BIN(00100011),0,0,0,BIN(010),"<http://www.ontologydesignpatterns.org/ont/dul/hasValue>",
			char op1[] = {14, 79, 1, 2, 103, -125, 35, 0, 0, 0, 2, 122, -98, 87, -99};
			process_nqxe_message(op1);
			// 1,3,'j',LEFT | 5,BIN(00100011),0,0,0,LEFT_COL(0) | RIGHT_COL(0),
			char op2[] = {10, 79, 1, 3, 106, 5, 35, 0, 0, 0, 0};
			process_nqxe_message(op2);
			// 1,4,'g',RIGHT | 5,BIN(00110011),0,0,0,BIN(010),"<http://purl.oclc.org/NET/ssnx/ssn#featureOfInterest>",
			char op3[] = {14, 79, 1, 4, 103, -123, 51, 0, 0, 0, 2, -56, -77, -12, 58};
			process_nqxe_message(op3);
			// 1,5,'j',LEFT | 7,BIN(11001011),0,0,0,LEFT_COL(0) | RIGHT_COL(0),
			char op4[] = {10, 79, 1, 5, 106, 7, -53, 0, 0, 0, 0};
			process_nqxe_message(op4);
			// 1,6,'g',RIGHT | 7,BIN(00000011),0,0,0,BIN(110),"<http://www.ontologydesignpatterns.org/ont/dul/hasLocation>","<http://me.exmpl/DERI>",
			char op5[] = {18, 79, 1, 6, 103, -121, 3, 0, 0, 0, 6, -95, 18, -76, 121, -24, 60, -50, -19};
			process_nqxe_message(op5);
			// 1,7,'j',LEFT | 8,BIN(00111000),0,0,0,LEFT_COL(2) | RIGHT_COL(0),
			char op6[] = {10, 79, 1, 7, 106, 8, 56, 0, 0, 0, 32};
			process_nqxe_message(op6);
			// 1,8,'a',LEFT | 0,BIN(11010110),0,0,0,4,AGAIN | 2,AGAIN | 4,5,0,
			char op7[] = {14, 79, 1, 8, 97, 0, -42, 0, 0, 0, 4, -126, -124, 5, 0};
			process_nqxe_message(op7);
			char q[] = { 3, 'Q', 1, 8};
			process_nqxe_message(q);
			
			ian_.reg_row_callback<ExampleApplication, &ExampleApplication::print_result_row>(this);
		}
		
		void print_result_row(int type, Processor::size_type cols, Processor::RowT& row, Processor::query_id_t qid, Processor::operator_id_t oid) {
			debug_->debug("ROW type %d qid %d oid %d", (int)type, (int)qid, (int)oid);
			for(size_t i = 0; i < cols; i++) {
				debug_->debug("  %08lx %f", (unsigned long)row[i], *reinterpret_cast<float*>(&row[i]));
			}
		}
		
		
		
		
		void query_all_p1() { query_all_p(0); }
		void query_all_p(void*) {
			ian_.set_exec_done_callback(Processor::exec_done_callback_t::from_method<ExampleApplication, &ExampleApplication::query_all_p1>(this));
			timer_->set_timer<ExampleApplication, &ExampleApplication::query_all>(1000, this, 0);
		}
		void query_all(void*) {
			enum { Q = Communicator::MESSAGE_ID_QUERY, OP = Communicator::MESSAGE_ID_OPERATOR };
			enum { ROOT = 0 };
			enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
			block_data_t qid = 1;
			
			
			//block_data_t op100[] = { OP, qid, 100, 'a', ROOT, BIN(010101), BIN(0), BIN(0), BIN(0), 3, MIN | AGAIN, AVG | AGAIN, MAX };
			block_data_t op100[] = { OP, qid, 100, 'c', ROOT, BIN(111111), BIN(0), BIN(0), BIN(0) };
			
			process(sizeof(op100), op100);
			
			block_data_t op70[]  = { OP, qid,  70, 'g', LEFT | 100, BIN(111111), BIN(0), BIN(0), BIN(0), BIN(000) };
			process(sizeof(op70), op70);
			
			block_data_t cmd[]   = { Q, qid, 2 };
			process(sizeof(cmd), cmd);
			
			ian_.erase_query(qid);
			
			//timer_->set_timer<ExampleApplication, &ExampleApplication::query_all>(1000, this, 0);
		}
		
		void query_cross_p1() { query_cross_p(0); }
		void query_cross_p(void*) {
			ian_.set_exec_done_callback(Processor::exec_done_callback_t::from_method<ExampleApplication, &ExampleApplication::query_cross_p1>(this));
			timer_->set_timer<ExampleApplication, &ExampleApplication::query_cross>(1000, this, 0);
		}
		
		void query_cross(void*) {
			enum { Q = Communicator::MESSAGE_ID_QUERY, OP = Communicator::MESSAGE_ID_OPERATOR };
			enum { ROOT = 0 };
			enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
			enum {
				LEFT_COLUMN_INVALID = 0x0f,
				RIGHT_COLUMN_INVALID = 0x0f
			};
			
			#if defined(ISENSE)
			GET_OS.clock().watchdog_stop();
			#endif
			
			
			//debug_->debug("qry");
			monitor_.report("qry");
			
			block_data_t qid = 1;
			block_data_t op100[] = { OP, qid, 100, 'c', ROOT, BIN(111111), BIN(1111), BIN(0), BIN(0) };
			
			process(sizeof(op100), op100);
			
			block_data_t op90[]  = { OP, qid,  90, 'j', LEFT | 100, BIN(11111111), BIN(1111), BIN(0), BIN(0), 0xff};
			process(sizeof(op90), op90);
			
			block_data_t op80[]  = { OP, qid,  80, 'g', RIGHT | 90, BIN(111111), BIN(0), BIN(0), BIN(0), BIN(000) };
			
			process(sizeof(op80), op80);
			block_data_t op70[]  = { OP, qid,  70, 'g', LEFT | 90, BIN(111111), BIN(0), BIN(0), BIN(0), BIN(000) };
			process(sizeof(op70), op70);
			
			block_data_t cmd[]   = { Q, qid, 4 };
			process(sizeof(cmd), cmd);
			
			ian_.erase_query(qid);
			
			debug_->debug("/qry");
			
			//timer_->set_timer<ExampleApplication, &ExampleApplication::query_cross>(1000, this, 0);
		}
		
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wwrite-strings"
		void be() {
			//init_ts();
			
			/*
			ins(ts, "A", "measures", "m1");
			ins(ts, "A", "measures", "m2");
			ins(ts, "m1", "has_value", "12");
			ins(ts, "m2", "has_value", "14");
			ins(ts, "B", "measures", "mb1");
			ins(ts, "B", "measures", "mb2");
			ins(ts, "mb1", "has_value", "20");
			ins(ts, "mb2", "has_value", "24");
			*/
			insert_tuples();
			
			ian_.init(&ts, timer_);
			communicator_.init(ian_, query_radio_, result_radio_, fndradio_, *timer_);
			communicator_.set_sink(SINK);
			
			//ian_.reverse_translator().fill();
			
			//pradio_.reg_recv_callback<ExampleApplication, &ExampleApplication::node_receive_query>( this );
			debug_->debug("rtr");
		}
		#pragma GCC diagnostic pop
		
		void sink_ask_hash_resolve(void*) {
			result_radio_.reg_recv_callback<ExampleApplication, &ExampleApplication::sink_receive_answer>( this );
			
			block_data_t msg[] = {
				Communicator::MESSAGE_ID_RESOLVE_HASHVALUE,
				0xf4, 0x59, 0xe3, 0x2b,
			};
			send(sizeof(msg), msg);
			query_radio_.flush();
		}
		
		void be_sink(void*) {
			//init_ts();
			//dictionary.init(debug_);
			//ts.init(&dictionary, &container, debug_);
			ian_.init(&ts, timer_);
			communicator_.init(ian_, query_radio_, result_radio_, fndradio_, *timer_);
			communicator_.set_sink(SINK);
			
			// set self as parent.
			// when we receive packets from ourself, we play sink, otherwise
			// we play in-network node
			fndradio_.set_parent(SINK);
			
			result_radio_.reg_recv_callback<ExampleApplication, &ExampleApplication::sink_receive_answer>( this );
			
#ifdef SHAWN
			timer_->set_timer<ExampleApplication, &ExampleApplication::sink_send_ssp_query>(10000, this, 0);
#else
			timer_->set_timer<ExampleApplication, &ExampleApplication::sink_send_query>(10000, this, 0);
#endif
		}
		
		void sink_send_ssp_query(void*) {
			block_data_t qid = 1;
			enum { Q = Communicator::MESSAGE_ID_QUERY, OP = Communicator::MESSAGE_ID_OPERATOR };
			enum { ROOT = 0 };
			enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
			
			block_data_t op100[] = { OP, qid, 100, 'a', ROOT, BIN(101010), 0, 0, 0, 3, MIN | AGAIN, AVG | AGAIN, MAX };
			send(sizeof(op100), op100);
			
			block_data_t op80[]  = { OP, qid,  80, 'g', LEFT | 100, BIN(100000), BIN(0), BIN(0), BIN(0), BIN(010), 0x4d, 0x0f, 0x60, 0xb4 };
			send(sizeof(op80), op80);
			
			block_data_t cmd[] = { Q, qid, 2 };
			send(sizeof(cmd), cmd);
			
			query_radio_.flush();
		}
		
		void sink_send_query(void*) {
			
			/*
			 * MIN(?v) MEAN(?v) MAX(?v) {
			 *    ?sens <http://purl.oclc.org/NET/ssnx/ssn#observedProperty> <http://spitfire-project.eu/property/Temperature> .
			 *    ?sens <http://www.loa-cnr.it/ontologies/DUL.owl#hasValue> ?v .
			 * }
			 * 

			<http://www.loa-cnr.it/ontologies/DUL.owl#hasValue>          4d0f60b4
			<http://spitfire-project.eu/property/Temperature>            b23860b3
			<http://purl.oclc.org/NET/ssnx/ssn#observedProperty>         bf26b82e

			 */
			
			enum { Q = Communicator::MESSAGE_ID_QUERY, OP = Communicator::MESSAGE_ID_OPERATOR };
			enum { ROOT = 0 };
			enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
			block_data_t qid = 1;
			
			
			block_data_t op100[] = { OP, qid, 100, 'a', ROOT, BIN(010101), BIN(0), BIN(0), BIN(0), 3, MIN | AGAIN, AVG | AGAIN, MAX };
			send(sizeof(op100), op100);
			
			block_data_t op90[]  = { OP, qid,  90, 'j', LEFT | 100, BIN(010000), BIN(0), BIN(0), BIN(0), LEFT_COL(0) | RIGHT_COL(0) };
			send(sizeof(op90), op90);
			
			block_data_t op80[]  = { OP, qid,  80, 'g', RIGHT | 90, BIN(010011), BIN(0), BIN(0), BIN(0), BIN(010), 0x4d, 0x0f, 0x60, 0xb4 };
			send(sizeof(op80), op80);
			
			block_data_t op70[]  = { OP, qid,  70, 'g', LEFT | 90, BIN(11), BIN(0), BIN(0), BIN(0), BIN(110), 0xbf, 0x26, 0xb8, 0x2e, 0xb2, 0x38, 0x60, 0xb3 };
			send(sizeof(op70), op70);
			
			block_data_t cmd[]   = { Q, qid, 4 };
			send(sizeof(cmd), cmd);
			
			query_radio_.flush();
			//timer_->set_timer<ExampleApplication, &ExampleApplication::sink_send_query>(10000, this, 0);
			
			/*
			 * Some hash values:
			 * // {{{
	
			10.2                                                         ff9ee843
			12-04-02T12:48Z                                              1b07abf2
			<http://purl.oclc.org/NET/muo/muo#UnitOfMeasurement>         bebc18a8
			<http://purl.oclc.org/NET/muo/muo#measuredIn>                078d9668
			<http://purl.oclc.org/NET/muo/muo#prefSymbol>                3fcd6c38
			<http://purl.oclc.org/NET/ssnx/ssn#Property>                 c41d15d6
			<http://purl.oclc.org/NET/ssnx/ssn#Sensor>                   7e1d99b1
			<http://purl.oclc.org/NET/ssnx/ssn#Stimulus>                 641c1231
			<http://purl.oclc.org/NET/ssnx/ssn#detects>                  18231901
			<http://purl.oclc.org/NET/ssnx/ssn#hasMeasurementCapability> 7ed3bfb1
			<http://purl.oclc.org/NET/ssnx/ssn#hasMeasurementProperty>   5b80e314
			<http://purl.oclc.org/NET/ssnx/ssn#isProxyFor>               bc98b186
			<http://purl.oclc.org/NET/ssnx/ssn#observedProperty>         bf26b82e
			<http://purl.org/dc/terms/date>                              62d5797c
			<http://spitfire-project.eu/cc/spitfireCC_n3.owl#uomInUse>   3d490f0a
			<http://spitfire-project.eu/property/Battery_Life_Time>      0dd47ba9
			<http://spitfire-project.eu/property/Temperature>            b23860b3
			<http://spitfire-project.eu/sensor/sensor1234/capabilities>  3fd78899
			<http://spitfire-project.eu/sensor/sensor1234/capabilities_sensor1234> 5b8d5808
			<http://spitfire-project.eu/sensor/sensor1234>               170b9e7e
			<http://spitfire-project.eu/sensor_stimulus/silver_expansion> c12e5e70
			<http://spitfire-project.eu/uom/Centigrade>                  24e0767b
			<http://spitfire-project.eu/uom/month>                       1d93598b
			<http://www.loa-cnr.it/ontologies/DUL.owl#hasValue>          4d0f60b4
			<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>            8b477de0
			<http://www.w3.org/2000/01/rdf-schema#subClassOf>            d3e5ee53
			<http://www.w3.org/2000/01/rdf-schema#type>                  b5f766c8
			<http://www.w3.org/2002/07/owl#Class>                        c632c40e
			<http://www.w3.org/2002/07/owl#Restriction>                  2e94a87a
			<http://www.w3.org/2002/07/owl#intersectionOf>               88bdb576
			<http://www.w3.org/2002/07/owl#onProperty>                   003aed82
			<http://www.w3.org/2002/07/owl#someValuesFrom>               51b52a36
			C                                                            00000043
			bnode0                                                       0aa5b6cc
			bnode1                                                       0aa5b6cd
			bnode2                                                       0aa5b6ce
			m                                                            0000006d
			
			* // }}}
			* 
			*/
			
		#if 0
			// {{{
			block_data_t op0[] = {
				Communicator::MESSAGE_ID_QUERY,
				1, // query id
				4, // number of operators
			};
			send(sizeof(op0), op0);
			
			/*
			block_data_t op1[] = {
				Communicator::MESSAGE_ID_OPERATOR,
				1, // query id
				100,  // op id
				'c',  // collect
				0,    // no parent
				BIN(01), 0, 0, 0, // projection info
			};
			send(sizeof(op1), op1);
			*/
			
			block_data_t op1[] = {
				Communicator::MESSAGE_ID_OPERATOR,
				1, // qid
				100, // op id
				'a', // aggregate
				0, // no parent
				BIN(0111), 0, 0, 0, // proj info
				5, // number of bytes following
				//enum AggregationType { GROUP = 0, SUM = 1, AVG = 2, COUNT = 3, MIN = 4, MAX = 5 };
				0, AGAIN | 4, AGAIN | 5, AGAIN | 1, 2
			};
			send(sizeof(op1), op1);
			
			block_data_t op2[] = {
				Communicator::MESSAGE_ID_OPERATOR,
				1, // query id
				90,  // id
				'j', // simple local join
				LEFT | 100, // parent id & port
				BIN(01000011), 0, 0, 0, // projection info
				LEFT_COL(1) | RIGHT_COL(0), // left col & right col
			};
			send(sizeof(op2), op2);
			
			block_data_t op3[] = {
				Communicator::MESSAGE_ID_OPERATOR,
				1, // query id
				80, // id
				'g', // graph pattern selection
				RIGHT | 90,   // parent offset & port
				BIN(00010011), 0, 0, 0, // projection info
				BIN(010), // affects predicate
				0xd6, 0x88, 0x14, 0xad, // "has_value"
			};
			send(sizeof(op3), op3);
			
			block_data_t op4[] = {
				Communicator::MESSAGE_ID_OPERATOR,
				1, // query id
				70, // id
				'g', // graph pattern selection
				LEFT | 90,   // parent offset & port
				BIN(00110011), 0x00, 0x00, 0x00, // projection info
				BIN(010), // affects predicate
				0x08, 0xff, 0xea, 0xc4, // "measures"
			};
			send(sizeof(op4), op4);
			
			
			
			//block_data_t exec[] = {
				//Communicator::MESSAGE_ID_EXECUTE,
				//1, // query id
			//};
			//send(sizeof(exec), exec);
			query_radio_.flush();
			
			//timer_->set_timer<ExampleApplication, &ExampleApplication::sink_ask_hash_resolve>(10000, this, 0);
			// }}}
		#endif
			
		}
		
		void send(size_t len, block_data_t *data) {
			query_radio_.send(SINK, len, data);
		}
		
		void sink_receive_answer( PAnsRadio::node_id_t from, PAnsRadio::size_t len, PAnsRadio::block_data_t *buf ) {
			/*
			PAnsRadio::message_id_t msgid = wiselib::read<Os, block_data_t, PRadio::message_id_t>(buf);
			
			debug_->debug("sink recv %d -> %d", from, result_radio_.id());
			
			if(from == SINK) {
				debug_->debug("sink recv from %d", from);
				wiselib::debug_buffer<Os, 16, Os::Debug>(debug_, buf, len);
			}
			*/
		}
		
		void print_memstat() {
			debug_->debug("rad %d tim %d dbg %d clk %d",
					(int)sizeof(Os::Radio), (int)sizeof(Os::Timer), (int)sizeof(Os::Debug),
					(int)sizeof(Os::Clock));
			debug_->debug("qrad %d fndrad %d pansrad %d trad %d",
					(int)sizeof(PRadio), (int)sizeof(FNDRadio), (int)sizeof(PAnsRadio),
					(int)sizeof(TRadio));
			debug_->debug("proc %d com %d alloc %d",
					(int)sizeof(Processor), (int)sizeof(Communicator), (int)sizeof(Allocator));
		}
		
	private:
		Os::Radio::self_pointer_t radio_;
		Os::Timer::self_pointer_t timer_;
		Os::Debug::self_pointer_t debug_;
		Os::Clock::self_pointer_t clock_;
		
		PRadio query_radio_;
		FNDRadio fndradio_;
		
		PAnsRadio result_radio_;
		TRadio tradio_;
		
		Processor ian_;
		Communicator communicator_;
		
		#if defined(ARDUINO)
			ArduinoMonitor<Os, Os::Debug> monitor_;
		#elif defined(ISENSE)
			IsenseMonitor<Os, Os::Debug> monitor_;
		#else
			NullMonitor<Os> monitor_;
		#endif
		
		
		#if INQP_TEST_USE_BLOCK
			BlockMemory block_memory_;
			BlockAllocator block_allocator_;
		#endif
};

Allocator allocator_;
Allocator& get_allocator() { return allocator_; }
// --------------------------------------------------------------------------
wiselib::WiselibApplication<Os, ExampleApplication> example_app;
// --------------------------------------------------------------------------
void application_main( Os::AppMainParameter& value )
{
  example_app.init( value );
}
