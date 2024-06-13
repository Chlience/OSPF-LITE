#ifndef NEIGHBOR_H
#define NEIGHBOR_H

#include <stdint.h>

enum struct NeighborState : uint8_t {
    S_DOWN = 0,
    S_ATTEMPT,
    S_INIT,
    S_2WAY,
    S_EXSTART,
    S_EXCHANGE,
    S_LOADING,
    S_FULL,
};

enum struct NeighborEvent : uint8_t {
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

class Neighbor {
	public:
	NeighborState	state = NeighborState::S_DOWN;
	bool			is_master;
	uint32_t		dd_seq_num;
	uint32_t		last_recv_dd_seq_num;
	uint32_t		id;		// router_id
	uint32_t		pri;
	uint32_t		ip;
	uint32_t		opts;
	uint32_t		dr;
	uint32_t		bdr;
	uint32_t		link_state_retrans_list;
	uint32_t		database_summary_list;
	uint32_t		link_state_request_list;
};

#endif