/*********************************************************************
    Filename:       RefNode.h
    Revised:        $Date: 2006-11-01 15:59:25 -0700 (Wed, 01 Nov 2006) $
    Revision:       $Revision: 12478 $

    Description: Reference Node for the Z-Stack Location Profile.

    Copyright (c) 2006 by Texas Instruments, Inc.
    All Rights Reserved.  Permission to use, reproduce, copy, prepare
    derivative works, modify, distribute, perform, display or sell this
    software and/or its documentation for any purpose is prohibited
    without the express written consent of Texas Instruments, Inc.
*********************************************************************/
#ifndef SINNODE_H
#define SINNODE_H

/*********************************************************************
 * INCLUDES
 */

#include "ZComDef.h"

/*********************************************************************
 * CONSTANTS
 */
#define SINK_BROADCAST_EVT                0x0001   //����ʹ��0x0008 �������������¼��г�ͻ

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
extern byte Sink_TaskID;

/*********************************************************************
 * FUNCTIONS
 */
void TurnP0CallBack( uint8 timerId, uint8 channel, uint8 channelMode); 
/*
 * Task Initialization for the Location Application - Reference Node
 */
void Sink_Init( byte task_id );

/*
 * Task Event Processor for the Location Application - Reference Node
 */
UINT16 Sink_ProcessEvent( byte task_id, UINT16 events );

#endif  // #ifndef REFNODE_H



/*********************************************************************
*********************************************************************/
