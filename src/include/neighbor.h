#ifndef NEIGHBOR_H
#define NEIGHBOR_H

#include <stdint.h>
#include <netinet/ip.h>
#include <pthread.h>

#include "link_state.h"

enum struct NeighborState:uint8_t {
    S_DOWN = 0,
    S_ATTEMPT,
    S_INIT,
    S_2WAY,
    S_EXSTART,
    S_EXCHANGE,
    S_LOADING,
    S_FULL,
};

enum struct NeighborEvent:uint8_t {
    E_HELLORECV = 0,	/* 从邻居接收到一个 Hello 包 */
    E_START,			/**
						 * 表示将以 HelloInterval 秒的间隔向邻居发送 Hello 包。
						 * 这一事件仅与 NBMA 网络上的邻居相关
						 */
    E_2WAYRECV,			/**
						 * 两台邻居路由器之间达到双向通讯。
						 * 这表明在邻居的 Hello 包中包含了路由器自身。
						 */
    E_NEGOTIATIONDONE,	/**
						 * 已经协商好主从关系，并交换了 DD 序号。
						 * 这一信号表示开始收发 DD 包。
						 * 生成这一事件的细节，参见第 10.8 节。
						 */
    E_EXCHANGEDONE,		/** 
						 * 两台路由器都已成功交换了完整的 DD 包。
						 * 每台路由器也知道其连接状态数据库中过期的部分。
						 * 生成这一事件的细节，参见第 10.8 节。
						 */
    E_BADLSREQ,			/**
						 * 接收到的连接状态请求中，包含有并不存在于数据库中的 LSA。
						 * 这说明在数据库交换过程中出现了错误。
						 */
    E_LOADINGDONE,		/**
						 * 连接状态更新已经接收了数据库中所有需要更新的部分。
						 * 这是由数据库交换过程完成后，连接状态请求列表为空而表明的。
						 */
    E_ADJOK,			/**
						 * 决定是否需要与邻居建立 / 维持邻接关系。
						 * 这将导致一些邻接的形成和拆除。
						 */
	/* 下面的事件会导致邻居状态的降低。与上面的事件不同，这些事件会在任何邻居会话的状
态时发生。 */
    E_SEQNUMMISMATCH,	/**
						* 接收到的 DD 包出现下列情况：
						* a）含有意外的 DD 序号；b）意外地设定了 Init 位；
						* c）与上一个接收到的 DD 包有着不同的选项域。
						* 这些情况都说明，在建立邻接的过程中出现了错误。
						*/
    E_1WAY,				/**
						 * 从邻居接收到 Hello 包，但并不包含路由器自身。
						 * 这说明与该邻居的通讯不再是双向。
						 */
    E_KILLNBR,			/* 这说明现在不可能与该邻居有任何通讯，强制转换邻居状态到 Down。*/
    E_INACTTIMER,		/* 非活跃记时器被激活。这说明最近没有从邻居接收到 Hello 包。强制转换邻居状态到 Down。 */
    E_LLDOWN,			/**
						 * 由下层协议说明，邻居不可到达。
						 * 例如在 X.25 PDN 中，由于适当的原因或诊断会收到 X.25 clear，以表示邻居关闭。
						 * 强制转换邻居状态到 Down。
						 */
};

class Interface;

class Neighbor {
	public:
	NeighborState	state;
	/* 邻居是否为 master */
	bool			is_master;
	/* 当前被发往邻居的 DD 包序号 */
	uint32_t		dd_seq_num;
	/* Last received Database Description packet */
	uint32_t		last_recv_dd_seq_num;
	uint8_t			last_recv_dd_i : 1;
	uint8_t			last_recv_dd_m : 1;
	uint8_t			last_recv_dd_ms : 1;
    uint8_t     	last_recv_dd_other : 5;
	
    char        	last_send_dd_data[1024];
    uint32_t    	last_send_dd_data_len;

	uint32_t		id;
	uint8_t			pri;
	uint32_t		ip;
	uint8_t			options;
	uint32_t		dr;
	uint32_t		bdr;
	std::list<LSAHeader>	link_state_retransmission_list;	// flooding 而没有确认的列表
	std::list<LSAHeader>	database_summary_list;			// 需要通过 DD 包发送的 LSA 列表
	std::list<LSAHeader>	link_state_request_list;		// 通过 DD 包获得需要同步的的未知 LSA 或者已知 LSA 的最新拷贝
	Interface*		interface;

	pthread_t		empty_dd_sender;
	bool			is_empty_dd_sender_running;
	bool			empty_dd_sender_stop;

	bool			lsr_sender_created;
	pthread_t		lsr_sender;
	pthread_mutex_t lsr_mutex;
	pthread_cond_t	lsr_cond;

	pthread_t		retrans_sender;
	pthread_mutex_t retrans_mutex;

	timespec		last_recv_hello_time;

	bool operator < (const Neighbor& other) const {
        return ip < other.ip;
    }

	Neighbor(uint32_t ip):ip(ip) {
		state = NeighborState::S_DOWN;
		is_master	= false;			// EXSTART
		dd_seq_num	= 0;				// EXSTART
		last_recv_dd_seq_num	= 0;	// EXSTART
		last_send_dd_data_len 	= 0;	// EXSTART
		id		= 0;			
		options	= 0x2;
		pri		= 0;		// INIT
		dr		= 0;		// INIT
		bdr		= 0;		// INIT
		interface	= nullptr;
		empty_dd_sender				= 0;
		empty_dd_sender_stop		= false;
		is_empty_dd_sender_running	= false;
		lsr_sender_created	= false;
		lsr_sender		= 0;
		lsr_mutex		= PTHREAD_MUTEX_INITIALIZER;
		lsr_cond		= PTHREAD_COND_INITIALIZER;
		retrans_sender	= 0;
		retrans_mutex	= PTHREAD_MUTEX_INITIALIZER;
	}

	void event_hello_received();
	void event_2way_received();
	void event_1way_received();
	void event_adj_ok();
	void event_negotiation_done();
	void event_exchange_done();
	void event_loading_done();
	void event_seq_number_mismatch();
	void event_bad_ls_req();
	void event_inactivity_timer();
};

#endif