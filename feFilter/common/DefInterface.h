#ifndef _DEFINTERFACE_H_
#define _DEFINTERFACE_H_

//定义guid头文件一定要引下面两个文件
#include "objbase.h" 
#include "initguid.h" 

// {B7D6D008-470C-4BC1-8598-EA069C933B61}
DEFINE_GUID(CLSID_Parser, 
	0xb7d6d008, 0x470c, 0x4bc1, 0x85, 0x98, 0xea, 0x6, 0x9c, 0x93, 0x3b, 0x61);


#endif //_DEFINTERFACE_H_
