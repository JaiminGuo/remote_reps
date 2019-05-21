
#include <stdio.h>
#include "M1553CFC_lib.h"

HANDLE hCard;

#define gMSG_GAP 3000
#define gPERIOD  20

unsigned short M1553_CalcCmdWord(unsigned short RTAddr, 
                                 unsigned short Tx, 
                                 unsigned short SubAddr,
                                 unsigned short cnt)
{
	unsigned short CmdWord;
	
	CmdWord=0;
	CmdWord=(RTAddr<<11) | (Tx<<10) | (SubAddr<<5) |(cnt);
	
	return CmdWord;
}

void GetSN()
{
	DWORD SN=0;
	/* open card */	
	if(!M1553_Open(&hCard,0))
	{
		printf("\nOpen Card Failed!\n");
		return -1;
	}

	/* reset card */
	if(!M1553_Reset(hCard))
	{
		printf("Reset Card failed!\n");
		M1553_Close(hCard);
		return -1;
	}


	M1553_GetSerialNum(hCard,&SN);
	printf("\nSN = 0x%08x\n",SN);
	
	/*close card */
	M1553_Close(hCard);
}

int TestM1553CFC(void)
{

	INTERRUPT_MASK_REGISTER_STRUCT maskreg;
	RETRY_CASE_STRUCT RetryCase;
	RETRY_CHANNEL_SEL_STRUCT RetryChanSel;
	STOP_ON_ERR_STRUCT StopOnErr;
	STATUS_SET_STRUCT StatusSet;
	RT_TX_MODE_STRUCT TxMode={0};
	RT_STATUS_WORD_STRUCT StatusWord;
	MT_CMD_FILTER_TABLE_STRUCT MtFilter;
	SMSG_STRUCT smsg[10];
	RMSG_STRUCT rmsg_bc,rmsg_rt,rmsg_mt;
	int i,k;
	WORD MsgId,count;
	DWORD cnt=0,SN;
	WORD DataBuf[32],BCIndex,RTRIndex,RTTIndex,MTIndex,msgcnt;
	BYTE chno;

	BCIndex=0;
	RTRIndex=0;
	RTTIndex=3;
	MTIndex=0;
    msgcnt=0;

    chno=0;

	/* open card */	
	if(!M1553_Open(&hCard,0))
	{
		printf("\nOpen Card Failed!\n");
		return -1;
	}

	/* reset card */
	if(!M1553_Reset(hCard))
	{
		printf("Reset Card failed!\n");
		M1553_Close(hCard);
		return -1;
	}

	M1553_GetSerialNum(hCard,&SN);
	printf("\nSN = 0x0%x\n",SN);

	/* add timetag */
	if(!M1553_AddTimeTag(hCard,chno,TRUE))
	{
		printf("Add TimeTag failed\n");
		M1553_Close(hCard);
		return -1;
	}

	/* set response time out */
	if(!M1553_SetResponseTimeout(hCard,chno,200))
	{
		printf("Set response time out failed!\n");
		M1553_Close(hCard);
		return -1;
	}

	/* Set interrupt mask register */
	maskreg.BC_MsgOver=TRUE;
	maskreg.RT_RMsg=TRUE;
	maskreg.RT_TMsg=TRUE;
	M1553_SetIntMaskReg(hCard,chno,&maskreg);

	/* -------------------------
	** Bus Controller Mode
	*/
	/* BC Init*/
	BC_Init(hCard,chno);

	/* BC Retry */
	BC_Retry(hCard,chno,FALSE);

	/* BC Retry Number */
	BC_SetRetryNum(hCard,chno,0);

	/* BC Retry Condition */
	RetryCase.Retry_IF_MSGErr=FALSE;
	RetryCase.Retry_IF_StatusSet=FALSE;
	if(!BC_SetRetryCase(hCard,chno,&RetryCase))
	{
		printf("Set BC Retry Condition failed!\n");
		M1553_Close(hCard);
		return -1;
	}

	/* BC Retry Channel Select */
	RetryChanSel.Alter_Chan_On_Retry1=FALSE;
	RetryChanSel.Alter_Chan_On_Retry2=FALSE;
	if(!BC_RetryChanSel(hCard,chno,&RetryChanSel))
	{
		printf("Set BC Retry Channel failed!\n");
		M1553_Close(hCard);
		return -1;
	}

	/* BC message/frame stop on error */
	StopOnErr.FRAME_STOP_ON_ERR=FALSE;
	StopOnErr.MSG_STOP_ON_ERR=FALSE;
	if(!BC_StopOnError(hCard,chno,&StopOnErr))
	{
		printf("BC message/frame stop on error failed!\n");
		M1553_Close(hCard);
		return -1;
	}
		
	/* BC message/frame on status set */
	StatusSet.Stop_On_Frame=FALSE;
	StatusSet.Stop_On_MSG=FALSE;
	if(!BC_OnStatusSet(hCard,chno, &StatusSet))
	{
		printf("BC message/frame on status set failed!\n");
		M1553_Close(hCard);
		return -1;
	}

	/* -------------------------
	** Remote Teminal Mode
	*/
	/* RT Init*/
	RT_Init(hCard,chno);

	RT_VectorClrEnable(hCard,chno,FALSE);

	/* RT Tx mode */
	RT_TxMode(hCard,chno,&TxMode);

	/* Load and Clear TimeTag */
	RT_ClearTTagOnSync(hCard,chno,TRUE);
	RT_LoadTTagOnSync(hCard,chno,TRUE);

	/* RT Status Word */
	StatusWord.TerminalFlag=FALSE;
	StatusWord.SubSystemFlag=FALSE;
	StatusWord.ServiceReq=FALSE;
	StatusWord.Busy=FALSE;
	StatusWord.DBusCtl=FALSE;
	for(i=0;i<32;i++)
		RT_Status_Set(hCard,chno,i,&StatusWord);

	/* RT Ileegal Command Enable */
	RT_IllegalCmd(hCard,chno,FALSE);
	RT_RevIllegalData(hCard,chno,TRUE);

	/* RT Enable */
	RT_Select(hCard,chno,0x7fffffff);

	/* -------------------------
	** Monitor Teminal Mode
	*/
	/* MT Init */
	MT_Init(hCard,chno);

	/* MT filter table */
	for(i=0;i<32;i++)
	{
		for(k=0;k<2;k++)
			MtFilter.Filter[i][k]=0xffffffff;
	}
	MT_SetCmdFilterTable(hCard,chno,&MtFilter);

	/* MT Start */
	MT_Start(hCard,chno,TRUE);


	/* bc message list */
	/* msg0: BC->RT */
	smsg[0].CtlWord.Retry=FALSE;
	smsg[0].CtlWord.ChanSel=1; /*chan A*/
	smsg[0].CtlWord.MsgFmt=0;
	smsg[0].MsgBlock.CmdWord1=M1553_CalcCmdWord(1,0,1,0);
	for(i=0;i<32;i++)
		smsg[0].MsgBlock.Datablk[i]=0x77ef+i;
	smsg[0].Gap=gMSG_GAP;
	smsg[0].Period=gPERIOD;
	smsg[0].Run=TRUE;
	BC_WriteMsg(hCard,chno,0,&smsg[0]);

	/* msg1: broadcast */
	smsg[1].CtlWord.Retry=FALSE;
	smsg[1].CtlWord.ChanSel=0; /*chan B*/
	smsg[1].CtlWord.MsgFmt=2;
	smsg[1].MsgBlock.CmdWord1=M1553_CalcCmdWord(31,0,15,0);
	for(i=0;i<32;i++)
		smsg[1].MsgBlock.Datablk[i]=0x7f77+i;
	smsg[1].Gap=gMSG_GAP;
	smsg[1].Period=gPERIOD;
	smsg[1].Run=TRUE;
	BC_WriteMsg(hCard,chno,1,&smsg[1]);

	/* msg2: sync with data */
	smsg[2].CtlWord.Retry=FALSE;
	smsg[2].CtlWord.ChanSel=1; /*chan A*/
	smsg[2].CtlWord.MsgFmt=4;
	smsg[2].MsgBlock.CmdWord1=M1553_CalcCmdWord(3,0,31,17);
	smsg[2].MsgBlock.Datablk[0]=0x1234;
	smsg[2].Gap=gMSG_GAP;
	smsg[2].Period=gPERIOD;
	smsg[2].Run=TRUE;
	BC_WriteMsg(hCard,chno,2,&smsg[2]);

	/* msg3: RT->BC */
	smsg[3].CtlWord.Retry=FALSE;
	smsg[3].CtlWord.ChanSel=0; /*chan B*/
	smsg[3].CtlWord.MsgFmt=0;
	smsg[3].MsgBlock.CmdWord1=M1553_CalcCmdWord(2,1,5,0);
	smsg[3].Gap=gMSG_GAP;
	smsg[3].Period=gPERIOD;
	smsg[3].Run=TRUE;
	BC_WriteMsg(hCard,chno,3,&smsg[3]);
	for(i=0;i<32;i++)
		DataBuf[i]=(WORD)(rand());
	RT_SendMSG(hCard,chno,2,5,32,DataBuf);

	/* msg4: Tx Vector Word */
	smsg[4].CtlWord.Retry=FALSE;
	smsg[4].CtlWord.ChanSel=0; /*chan B*/
	smsg[4].CtlWord.MsgFmt=4;
	smsg[4].MsgBlock.CmdWord1=M1553_CalcCmdWord(6,1,31,16);
	smsg[4].Gap=gMSG_GAP;
	smsg[4].Period=gPERIOD;
	smsg[4].Run=TRUE;
	BC_WriteMsg(hCard,chno,4,&smsg[4]);
	RT_SetVectorWord(hCard,chno,6,0x4444);

	BC_AddEndOfListFlag(hCard,chno,5);
	BC_Start(hCard,chno);

	msgcnt=5;

	while(1)
	{

		while(BC_IsMSGOver(hCard,chno))
		{
			if((cnt !=0) && ((cnt%500) == 0))
				printf("\n++++++ loop = %d\n",cnt);
			memset(&rmsg_bc,0,sizeof(rmsg_bc));
			BC_ReadNextMsg(hCard,chno,&MsgId,&rmsg_bc);

			/* check */
			if(rmsg_bc.MsgBlock.CmdWord1 != smsg[BCIndex].MsgBlock.CmdWord1)
			{
				BC_Stop(hCard,chno);
				count=BC_GetMsgNum_Newly(hCard,chno);
				printf("BC Command Word Error!\n");
				break;
			}

			if(((rmsg_bc.BSW) & 0x0200) != 0)
			{
				printf("BC BSW Error: %x\n",rmsg_bc.BSW);
				break;
			}

			if(BCIndex < 2)
			{
				for(i=0;i<32;i++)
				{
					if(rmsg_bc.MsgBlock.Datablk[i] != smsg[BCIndex].MsgBlock.Datablk[i])
					{
						printf("BC data Error!\n");
						break;
					}
				}
			}
			else if(BCIndex == 2)
			{
				if(rmsg_bc.MsgBlock.Datablk[0] != 0x1234)
				{
					printf("BC data Error!\n");
					break;
				}
			}
			else if(BCIndex == 3)
			{
				for(i=0;i<32;i++)
				{
					if(rmsg_bc.MsgBlock.Datablk[i] != DataBuf[i])
					{
						printf("BC data Error!\n");
						break;
					}
				}
			}
			else if(BCIndex == 4)
			{
				if(rmsg_bc.MsgBlock.Datablk[0] != 0x4444)
				{
					printf("BC data Error!\n");
					break;
				}
			}
			BCIndex++;
			if(BCIndex == msgcnt)
			{
				BCIndex=0;
			}
			/*printf("BC has send a message!\n");*/
			cnt++;
		}

		memset(&rmsg_rt,0,sizeof(rmsg_rt));
		if(RT_ReadMSG_Rx(hCard,chno,&rmsg_rt))
		{
		
			if(rmsg_rt.MsgBlock.CmdWord1 != smsg[RTRIndex].MsgBlock.CmdWord1)
			{
				printf("RT Command Word Error!\n");
				break;
			}
			if(RTRIndex < 2)
			{
				for(i=0;i<32;i++)
				{
					if(rmsg_rt.MsgBlock.Datablk[i] != smsg[RTRIndex].MsgBlock.Datablk[i])
					{
						printf("RT data Error!\n");
						break;
					}
				}
			}
			else if(RTRIndex == 2)
			{
				if(rmsg_rt.MsgBlock.Datablk[0] != 0x1234)
				{
					printf("RT data Error!\n");
					break;
				}
			}
			RTRIndex++;
			if(RTRIndex == 3)
				RTRIndex=0;

			
		}
		memset(&rmsg_rt,0,sizeof(rmsg_rt));
		if(RT_ReadMSG_Tx(hCard,chno,&rmsg_rt))
		{
			if(rmsg_rt.MsgBlock.CmdWord1 != smsg[RTTIndex].MsgBlock.CmdWord1)
			{
				printf("RT Command Word Error!\n");
				break;
			}
			if(RTTIndex == 3)
			{
				for(i=0;i<32;i++)
				{
					if(rmsg_rt.MsgBlock.Datablk[i] != DataBuf[i])
					{
						printf("RT data Error!\n");
						break;
					}
				}
			}
			else if(RTTIndex == 4)
			{
				if(rmsg_rt.MsgBlock.Datablk[0] != 0x4444)
				{
					printf("RT data Error!\n");
					break;
				}
			}
			RTTIndex++;
			if(RTTIndex == msgcnt)
				RTTIndex=3;

		
		}

		memset(&rmsg_mt,0,sizeof(rmsg_mt));
		if(MT_ReadMSG(hCard,chno,&rmsg_mt))
		{
			
			if(rmsg_mt.MsgBlock.CmdWord1 != smsg[MTIndex].MsgBlock.CmdWord1)
			{
				printf("MT Command Word Error!\n");
				break;
			}
			if(MTIndex < 2)
			{
				for(i=0;i<32;i++)
				{
					if(rmsg_mt.MsgBlock.Datablk[i] != smsg[MTIndex].MsgBlock.Datablk[i])
					{
						printf("MT data Error!\n");
						break;
					}
				}
			}
			else if(MTIndex == 2)
			{
				if(rmsg_mt.MsgBlock.Datablk[0] != 0x1234)
				{
					printf("MT data Error!\n");
					break;
				}
			}
			else if(MTIndex == 3)
			{
				for(i=0;i<32;i++)
				{
					if(rmsg_mt.MsgBlock.Datablk[i] != DataBuf[i])
					{
						printf("MT data Error!\n");
						break;
					}
				}
			}
			else if(MTIndex == 4)
			{
				if(rmsg_mt.MsgBlock.Datablk[0] != 0x4444)
				{
					printf("MT data Error!\n");
					break;
				}
			}
			MTIndex++;
			if(MTIndex == msgcnt)
				MTIndex=0;
			
		}
	}

	M1553_Reset(hCard);

	/*close card */
	M1553_Close(hCard);
	
	return 0;
}

