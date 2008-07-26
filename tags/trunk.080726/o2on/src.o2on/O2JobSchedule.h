/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: schedule.h
 * description	: 
 *
 */

#pragma once

#define JOB_INTERVAL_PERFORMANCE_COUNTER	5
#define JOB_INTERVAL_GET_GLOBAL_IP			0
#define JOB_INTERVAL_QUERY_DAT				60
#define JOB_INTERVAL_DAT_COLLECTOR			60
#define JOB_INTERVAL_ASK_COLLECTION			60
#define JOB_INTERVAL_PUBLISH_KEYS			(2*60)
#define JOB_INTERVAL_PUBLISH_ORIGINAL		(O2DEBUG ? 10 : 60)
#define JOB_INTERVAL_COLLECT_NODE			15
#define JOB_INTERVAL_COLLECT_NODE2			(3*60)
#define JOB_INTERVAL_SEARCH					(O2DEBUG ? 10 : 60)
#define JOB_INTERVAL_SEARCHFRIENDS			30
#define JOB_INTERVAL_BROADCAST				30
#define JOB_INTERVAL_WORKSET_CLEAR			30
#define JOB_INTERVAL_AUTO_SAVE				(3*60)

#define COLLECT_NODE_THRESHOLD				3
#define DAT_COLLECTOR_INTERVAL_MS			1000
#define ASK_COLLECTION_INTERVAL_MS			1000
#define QUERY_DAT_INTERVAL_MS				1000

#define PUBLISH_INTERVAL_MS					500
#define PUBLISHNUM_PER_THREAD				500
#define PUBLISH_START_NODE_COUNT			10
#define PUBLISH_ORIGINAL_TT					(3*60*60)	/* 3 hour */
#define PUBLISH_TT							(30*60)		/* 30 min. */

#define SEARCH_INTERVAL_MS					1000
#define SEARCH_REFINE_INTERVAL_MS			500

#define O2_FRIEND_LIVING_TT					5*60