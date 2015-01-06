#ifndef BLINDNODE_H
#define BLINDNODE_H

/*********************************************************************
    Filename:       BlindNode.h
    Revised:        $Date: 2006-11-03 19:11:21 -0700 (Fri, 03 Nov 2006) $
    Revision:       $Revision: 12552 $

    Description:

       This file contains the BlindNode through App definitions.

    Notes:

    Copyright (c) 2006 by Texas Instruments, Inc.
    All Rights Reserved.  Permission to use, reproduce, copy, prepare
    derivative works, modify, distribute, perform, display or sell this
    software and/or its documentation for any purpose is prohibited
    without the express written consent of Texas Instruments, Inc.
*********************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"

/*********************************************************************
 * CONSTANTS
 */

// Blind Node's Events (bit masked)
#define MOBILE_TEMP_EVT                0x0001   //����ʹ��0x0008 �������������¼��г�ͻ

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
extern byte Mobile_TaskID;

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Task Initialization for the Location Application - Blind Node
 */
extern void Mobile_Init( byte task_id );

/*
 * Task Event Processor for the Location Application - Blind Node
 */
extern UINT16 Mobile_ProcessEvent( byte task_id, UINT16 events );

/*
 * Start a sequence of blasts and calculate position.
 */
extern void Mobile_FindRequest( void );

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* BLINDNODE_H */
