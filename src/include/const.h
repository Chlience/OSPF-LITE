#ifndef OSPF_CONST_H
#define OSPF_CONST_H

#include <cstdint>

#define LSR_REFRESH_TIME	(1800)
#define MIN_LS_INTERVAL		(5)
#define MIN_LS_ARRIVAL		(5)
#define MAX_AGE				(3600)
#define CHECK_AGE			(300)
#define MAX_AGE_DIFF		(900)
#define LS_INFINITY			(0xffffff)

const uint32_t INITIAL_SEQUENCE_NUMBER	= 0x80000001;
const uint32_t MAX_SEQUENCE_NUMBER 		= 0x7ffffff;

#endif