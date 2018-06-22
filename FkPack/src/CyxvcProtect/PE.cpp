#include "stdafx.h"
#include "PE.h"


CPE::CPE()
{
	InitValue();
}


CPE::~CPE()
{
}


void CPE::InitValue()
{
	m_hFile				= NULL;
	m_pFileBuf			= NULL;
	m_pDosHeader		= NULL;
	m_pNtHeader			= NULL;
	m_pSecHeader		= NULL;
	m_dwFileSize		= 0;
	m_dwImageSize		= 0;
	m_dwImageBase		= 0;
	m_dwCodeBase		= 0;
	m_dwCodeSize		= 0;
	m_dwPEOEP			= 0;
	m_dwShellOEP		= 0;
	m_dwSizeOfHeader	= 0;
	m_dwSectionNum		= 0;
	m_dwFileAlign		= 0;
	m_dwMemAlign		= 0;
	m_PERelocDir		= { 0 };
	m_PEImportDir		= { 0 };
	m_IATSectionBase	= 0;
	m_IATSectionSize	= 0;

	m_pMyImport			= 0;
	m_pModNameBuf		= 0;
	m_pFunNameBuf		= 0;
	m_dwNumOfIATFuns	= 0;
	m_dwSizeOfModBuf	= 0;
	m_dwSizeOfFunBuf	= 0;
	m_dwIATBaseRVA		= 0;
}


// 函数说明:	初始化PE，读取PE文件，保存PE信息
BOOL CPE::InitPE(CString strFilePath)
{
	//打开文件
	if (OpenPEFile(strFilePath) == FALSE)
		return FALSE;

	//将PE以文件分布格式读取到内存
	m_dwFileSize = GetFileSize(m_hFile, NULL);
	m_pFileBuf = new BYTE[m_dwFileSize];
	DWORD ReadSize = 0;
	ReadFile(m_hFile, m_pFileBuf, m_dwFileSize, &ReadSize, NULL);	
	CloseHandle(m_hFile);
	m_hFile = NULL;

	//判断是否为PE文件
	if (IsPE() == FALSE)
		return FALSE;

	//将PE以内存分布格式读取到内存
	//修正没镜像大小没有对齐的情况
	m_dwImageSize = m_pNtHeader->OptionalHeader.SizeOfImage;
	m_dwMemAlign = m_pNtHeader->OptionalHeader.SectionAlignment;
	m_dwSizeOfHeader = m_pNtHeader->OptionalHeader.SizeOfHeaders;
	m_dwSectionNum = m_pNtHeader->FileHeader.NumberOfSections;

	if (m_dwImageSize % m_dwMemAlign)
		m_dwImageSize = (m_dwImageSize / m_dwMemAlign + 1) * m_dwMemAlign;
	LPBYTE pFileBuf_New = new BYTE[m_dwImageSize];
	memset(pFileBuf_New, 0, m_dwImageSize);
	//拷贝文件头
	memcpy_s(pFileBuf_New, m_dwSizeOfHeader, m_pFileBuf, m_dwSizeOfHeader);
	//拷贝区段
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNtHeader);
	for (DWORD i = 0; i < m_dwSectionNum; i++, pSectionHeader++)
	{
		memcpy_s(pFileBuf_New + pSectionHeader->VirtualAddress,
			pSectionHeader->SizeOfRawData,
			m_pFileBuf+pSectionHeader->PointerToRawData,
			pSectionHeader->SizeOfRawData);
	}
	delete[] m_pFileBuf;
	m_pFileBuf = pFileBuf_New;
	pFileBuf_New = NULL;

	//获取PE信息
	GetPEInfo();
	
	return TRUE;
}


// 函数说明:	打开文件
BOOL CPE::OpenPEFile(CString strFilePath)
{
	m_hFile = CreateFile(strFilePath,
		GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		MessageBox(NULL, _T("加载文件失败！"), _T("提示"), MB_OK);
		m_hFile = NULL;
		return FALSE;
	}
	return TRUE;
}

// 函数说明:	判断是否为PE文件
BOOL CPE::IsPE()
{
	//判断是否为PE文件
	m_pDosHeader = (PIMAGE_DOS_HEADER)m_pFileBuf;
	if (m_pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
	{
		//不是PE文件
		MessageBox(NULL, _T("不是有效的PE文件！"), _T("提示"), MB_OK);
		delete[] m_pFileBuf;
		InitValue();
		return FALSE;
	}
	m_pNtHeader = (PIMAGE_NT_HEADERS)(m_pFileBuf + m_pDosHeader->e_lfanew);
	if (m_pNtHeader->Signature != IMAGE_NT_SIGNATURE)
	{
		//不是PE文件
		MessageBox(NULL, _T("不是有效的PE文件！"), _T("提示"), MB_OK);
		delete[] m_pFileBuf;
		InitValue();
		return FALSE;
	}
	return TRUE;
}

// 函数说明: RVA 转文件偏移
DWORD CPE::RvaToOffset(DWORD Rva)
{
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)m_pFileBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + m_pFileBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	for (int i = 0; i<pNt->FileHeader.NumberOfSections; i++)
	{
		if (Rva >= pSection->VirtualAddress&&
			Rva <= pSection->VirtualAddress + pSection->Misc.VirtualSize)
		{
			// 如果文件地址为0,将无法在文件中找到对应的内容
			if (pSection->PointerToRawData == 0)
			{
				return -1;
			}
			return Rva - pSection->VirtualAddress + pSection->PointerToRawData;
		}
		pSection = pSection + 1;
	}
}


// 函数说明:	获取PE信息
void CPE::GetPEInfo()
{
	m_pDosHeader	= (PIMAGE_DOS_HEADER)m_pFileBuf;
	m_pNtHeader		= (PIMAGE_NT_HEADERS)(m_pFileBuf + m_pDosHeader->e_lfanew);

	m_dwFileAlign	= m_pNtHeader->OptionalHeader.FileAlignment;
	m_dwMemAlign	= m_pNtHeader->OptionalHeader.SectionAlignment;
	m_dwImageBase	= m_pNtHeader->OptionalHeader.ImageBase;
	m_dwPEOEP		= m_pNtHeader->OptionalHeader.AddressOfEntryPoint;
	m_dwCodeBase	= m_pNtHeader->OptionalHeader.BaseOfCode;
	m_dwCodeSize	= m_pNtHeader->OptionalHeader.SizeOfCode;
	m_dwSizeOfHeader= m_pNtHeader->OptionalHeader.SizeOfHeaders;
	m_dwSectionNum	= m_pNtHeader->FileHeader.NumberOfSections;
	m_pSecHeader	= IMAGE_FIRST_SECTION(m_pNtHeader);
	m_pNtHeader->OptionalHeader.SizeOfImage = m_dwImageSize;

	//保存重定位目录信息
	m_PERelocDir = 
		IMAGE_DATA_DIRECTORY(m_pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]);

	//保存 IAT 目录信息
	m_PEImportDir =
		IMAGE_DATA_DIRECTORY(m_pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]);

	//获取 IAT 所在的区段的起始位置和大小
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNtHeader);
	for (DWORD i = 0; i < m_dwSectionNum; i++, pSectionHeader++)
	{
		if (m_PEImportDir.VirtualAddress >= pSectionHeader->VirtualAddress&&
			m_PEImportDir.VirtualAddress <= pSectionHeader[1].VirtualAddress)
		{
			//保存该区段的起始地址和大小
			m_IATSectionBase = pSectionHeader->VirtualAddress;
			m_IATSectionSize = pSectionHeader[1].VirtualAddress - pSectionHeader->VirtualAddress;
			break;
		}
	}
}

// 函数说明:	代码段加密
DWORD CPE::XorCode(BYTE byXOR)
{
	PBYTE pCodeBase = (PBYTE)((DWORD)m_pFileBuf + m_dwCodeBase);
	for (DWORD i = 0; i < m_dwCodeSize; i++)
	{
		pCodeBase[i] ^= i;
	}
	return m_dwCodeSize;
}

// 函数说明:	机器码绑定(将机器码同代码段进行亦或)
void CPE::XorMachineCode(CHAR MachineCode[16])
{
	PBYTE pCodeBase = (PBYTE)((DWORD)m_pFileBuf + m_dwCodeBase);
	DWORD j = 0;
	for (DWORD i = 0; i < m_dwCodeSize; i++)
	{
		pCodeBase[i] ^= MachineCode[j++];
		if (j==16)
			j = 0;
	}
}


// 函数说明:	设置Shell的重定位信息
BOOL CPE::SetShellReloc(LPBYTE pShellBuf, DWORD hShell)
{
	typedef struct _TYPEOFFSET
	{
		WORD offset : 12;			//偏移值
		WORD Type	: 4;			//重定位属性(方式)
	}TYPEOFFSET, *PTYPEOFFSET;

	//1.获取被加壳PE文件的重定位目录表指针信息
	PIMAGE_DATA_DIRECTORY pPERelocDir =
		&(m_pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]);
	
	//2.获取Shell的重定位表指针信息
	PIMAGE_DOS_HEADER		pShellDosHeader = (PIMAGE_DOS_HEADER)pShellBuf;
	PIMAGE_NT_HEADERS		pShellNtHeader = (PIMAGE_NT_HEADERS)(pShellBuf + pShellDosHeader->e_lfanew);
	PIMAGE_DATA_DIRECTORY	pShellRelocDir =
		&(pShellNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]);
	PIMAGE_BASE_RELOCATION	pShellReloc = 
		(PIMAGE_BASE_RELOCATION)((DWORD)pShellBuf + pShellRelocDir->VirtualAddress);
	
	//3.还原修复重定位信息
	//由于Shell.dll是通过LoadLibrary加载的，所以系统会对其进行一次重定位
	//我们需要把Shell.dll的重定位信息恢复到系统没加载前的样子，然后在写入被加壳文件的末尾
	while (pShellReloc->VirtualAddress)
	{		
		PTYPEOFFSET pTypeOffset = (PTYPEOFFSET)(pShellReloc + 1);
		DWORD dwNumber = (pShellReloc->SizeOfBlock - 8) / 2;

		for (DWORD i = 0; i < dwNumber; i++)
		{
			if (*(PWORD)(&pTypeOffset[i]) == NULL)
				break;
			//RVA
			DWORD dwRVA = pTypeOffset[i].offset + pShellReloc->VirtualAddress;
			//FAR地址
			//***新的重定位地址=重定位地址-映象基址+新的映象基址+代码基址
			DWORD AddrOfNeedReloc = *(PDWORD)((DWORD)pShellBuf + dwRVA);
			*(PDWORD)((DWORD)pShellBuf + dwRVA)
				= AddrOfNeedReloc - pShellNtHeader->OptionalHeader.ImageBase + m_dwImageBase + m_dwImageSize;
		}
		//3.1修改Shell重定位表中.text的RVA
		pShellReloc->VirtualAddress += m_dwImageSize;
		// 修复下一个区段
		pShellReloc = (PIMAGE_BASE_RELOCATION)((DWORD)pShellReloc + pShellReloc->SizeOfBlock);
	}

	//4.修改PE重定位目录指针，指向Shell的重定位表信息
	pPERelocDir->Size = pShellRelocDir->Size;
	pPERelocDir->VirtualAddress = pShellRelocDir->VirtualAddress + m_dwImageSize;

	return TRUE;
}


// 函数说明:	合并壳与被加壳程序
void CPE::MergeBuf(LPBYTE pFileBuf, DWORD pFileBufSize,
	LPBYTE pShellBuf, DWORD pShellBufSize, 
	LPBYTE& pFinalBuf, DWORD& pFinalBufSize)
{
	//获取最后一个区段的信息
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pFileBuf;
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)(pFileBuf + pDosHeader->e_lfanew);
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeader);
	PIMAGE_SECTION_HEADER pLastSection =
		&pSectionHeader[pNtHeader->FileHeader.NumberOfSections - 1];

	//1.修改区段数量
	pNtHeader->FileHeader.NumberOfSections += 1;

	//2.编辑区段表头结构体信息
	PIMAGE_SECTION_HEADER AddSectionHeader =
		&pSectionHeader[pNtHeader->FileHeader.NumberOfSections - 1];
	memcpy_s(AddSectionHeader->Name, 8, ".FKBUG", 6);

	//VOffset(1000对齐)
	DWORD dwTemp = 0;
	dwTemp = (pLastSection->Misc.VirtualSize / m_dwMemAlign) * m_dwMemAlign;
	if (pLastSection->Misc.VirtualSize % m_dwMemAlign)
	{
		dwTemp += m_dwMemAlign;
	}
	AddSectionHeader->VirtualAddress = pLastSection->VirtualAddress + dwTemp;

	//Vsize（实际添加的大小）
	AddSectionHeader->Misc.VirtualSize = pShellBufSize;

	//ROffset（旧文件的末尾）
	AddSectionHeader->PointerToRawData = pFileBufSize;

	//RSize(200对齐)
	dwTemp = (pShellBufSize / m_dwFileAlign) * m_dwFileAlign;
	if (pShellBufSize % m_dwFileAlign)
	{
		dwTemp += m_dwFileAlign;
	}
	AddSectionHeader->SizeOfRawData = dwTemp;

	//标志
	AddSectionHeader->Characteristics = 0XE0000040;

	//3.修改 PE 头文件大小属性，增加文件大小
	dwTemp = (pShellBufSize / m_dwMemAlign) * m_dwMemAlign;
	if (pShellBufSize % m_dwMemAlign)
	{
		dwTemp += m_dwMemAlign;
	}
	pNtHeader->OptionalHeader.SizeOfImage += dwTemp;


	//4.申请合并所需要的空间
	//4.0.计算保存IAT所用的空间大小
	DWORD dwIATSize = 0;
	dwIATSize = m_dwSizeOfModBuf + m_dwSizeOfFunBuf + m_dwNumOfIATFuns*sizeof(MYIMPORT);

	if (dwIATSize % m_dwMemAlign)
	{
		dwIATSize = (dwIATSize / m_dwMemAlign + 1)*m_dwMemAlign;
	}
	pNtHeader->OptionalHeader.SizeOfImage += dwIATSize;
	AddSectionHeader->Misc.VirtualSize += dwIATSize;
	AddSectionHeader->SizeOfRawData += dwIATSize;

	pFinalBuf = new BYTE[pFileBufSize + dwTemp + dwIATSize];
	pFinalBufSize = pFileBufSize + dwTemp + dwIATSize;
	memset(pFinalBuf, 0, pFileBufSize + dwTemp + dwIATSize);
	memcpy_s(pFinalBuf, pFileBufSize, pFileBuf, pFileBufSize);
	memcpy_s(pFinalBuf + pFileBufSize, dwTemp, pShellBuf, dwTemp);

	//拷贝IAT信息
	if (dwIATSize == 0)
	{
		return;
	}
	DWORD dwIATBaseRVA = pFileBufSize + pShellBufSize;
	m_dwIATBaseRVA = dwIATBaseRVA;

	memcpy_s(pFinalBuf + dwIATBaseRVA,
		dwIATSize, m_pMyImport, m_dwNumOfIATFuns*sizeof(MYIMPORT));

	//加密模块名
	for (DWORD i = 0; i < m_dwSizeOfModBuf; i++)
	{
		if (((char*)m_pModNameBuf)[i] != 0)
		{
			((char*)m_pModNameBuf)[i] ^= 0x15;
		}
	}

	memcpy_s(pFinalBuf + dwIATBaseRVA + m_dwNumOfIATFuns*sizeof(MYIMPORT),
		dwIATSize, m_pModNameBuf, m_dwSizeOfModBuf);

	//IAT函数名加密
	for (DWORD i = 0; i < m_dwSizeOfFunBuf; i++)
	{
		if (((char*)m_pFunNameBuf)[i] != 0)
		{
			((char*)m_pFunNameBuf)[i] ^= 0x15;
		}
	}

	memcpy_s(pFinalBuf + dwIATBaseRVA + m_dwNumOfIATFuns*sizeof(MYIMPORT)+m_dwSizeOfModBuf,
		dwIATSize, m_pFunNameBuf, m_dwSizeOfFunBuf);
}


// 函数说明:	修改新的OEP为Shell的Start函数
void CPE::SetNewOEP(DWORD dwShellOEP)
{
	m_dwShellOEP = dwShellOEP + m_dwImageSize;
	m_pNtHeader->OptionalHeader.AddressOfEntryPoint = m_dwShellOEP;
}



// 函数说明:	抹去IAT(导入表)数据
void CPE::ClsImportTab()
{
	if (m_PEImportDir.VirtualAddress == 0)
	{
		return;
	}
	//1.获取导入表结构体指针
	PIMAGE_IMPORT_DESCRIPTOR pPEImport =
		(PIMAGE_IMPORT_DESCRIPTOR)(m_pFileBuf + m_PEImportDir.VirtualAddress);

	//2.开始循环抹去IAT(导入表)数据
	//每循环一次抹去一个Dll的所有导入信息
	while (pPEImport->Name)
	{
		//2.1.抹去模块名
		DWORD dwModNameRVA = pPEImport->Name;
		char* pModName = (char*)(m_pFileBuf + dwModNameRVA);
		memset(pModName, 0, strlen(pModName));

		PIMAGE_THUNK_DATA pIAT = (PIMAGE_THUNK_DATA)(m_pFileBuf + pPEImport->FirstThunk);
		PIMAGE_THUNK_DATA pINT = (PIMAGE_THUNK_DATA)(m_pFileBuf + pPEImport->OriginalFirstThunk);

		//2.2.抹去IAT、INT和函数名函数序号
		while (pIAT->u1.AddressOfData)
		{
			//判断是输出函数名还是序号
			if (IMAGE_SNAP_BY_ORDINAL(pIAT->u1.Ordinal))
			{
				//抹去序号就是将pIAT清空
			}
			else
			{
				//输出函数名
				DWORD dwFunNameRVA = pIAT->u1.AddressOfData;
				PIMAGE_IMPORT_BY_NAME pstcFunName = (PIMAGE_IMPORT_BY_NAME)(m_pFileBuf + dwFunNameRVA);
				//清除函数名和函数序号
				memset(pstcFunName, 0, strlen(pstcFunName->Name) + sizeof(WORD));
			}
			memset(pINT, 0, sizeof(IMAGE_THUNK_DATA));
			memset(pIAT, 0, sizeof(IMAGE_THUNK_DATA));
			pINT++;
			pIAT++;
		}

		//2.3.抹去导入表目录信息
		memset(pPEImport, 0, sizeof(IMAGE_IMPORT_DESCRIPTOR));

		//遍历下一个模块
		pPEImport++;
	}
}


// 函数说明: 处理 TLS 回调函数
DWORD CPE::DealWithTLS(PSHELL_DATA & pPackInfo)
{
	if (m_pNtHeader->OptionalHeader.DataDirectory[9].VirtualAddress == 0)
	{
		return false;
	}
	else
	{
		PIMAGE_TLS_DIRECTORY32 g_lpTlsDir =
			(PIMAGE_TLS_DIRECTORY32)
			(RvaToOffset(m_pNtHeader->OptionalHeader.DataDirectory[9].VirtualAddress) + m_pFileBuf);

		// 获取 TLSIndex 的 文件偏移
		DWORD IndexOfOffset = RvaToOffset(g_lpTlsDir->AddressOfIndex - m_dwImageBase);

		// 设置 TLSIndex 的值
		pPackInfo->TLSIndex = 0;
		if (IndexOfOffset != -1)
		{
			pPackInfo->TLSIndex = *(DWORD*)(IndexOfOffset + m_pFileBuf);
		}

		// 设置 TLS 表中的信息
		m_StartOfDataAddress = g_lpTlsDir->StartAddressOfRawData;
		m_EndOfDataAddresss = g_lpTlsDir->EndAddressOfRawData;
		m_CallBackFuncAddress = g_lpTlsDir->AddressOfCallBacks;

		// 将 TLS 回调函数 rva 设置到共享信息结构体
		pPackInfo->TLSCallBackFuncRva = m_CallBackFuncAddress;
		return true;
	}
}

// 函数说明: 设置 TLS 回调函数
// 1. 将 被加壳程序的 TLS 数据目录表指向壳的
// 2. 通过 TLS 表中的 StartAddressOfRawData 在区段中寻找
// 3. 将 Index 索引存入壳与加壳器之间交互的数据结构,计算这个变量的 RVA(在重定位后设置为 Rva-代码段基址+FKBUG段基址+被加壳程序加载基址)
// 4. 壳的 TLS 表前两项同 被加壳程序的 TLS 表, 但数值上需要在重定位之后设置为和被加壳程序的 TLS 表项相同
// 5. 壳的 AddressOfFunc 同被加壳程序的 TLS 表, 但数值上也是需要重定位之后设为与被加壳程序相同
void CPE::SetTLS(DWORD NewSectionRva, PCHAR pStubBuf, PSHELL_DATA pPackInfo)
{
	PIMAGE_DOS_HEADER pStubDos = (PIMAGE_DOS_HEADER)pStubBuf;
	PIMAGE_NT_HEADERS pStubNt = (PIMAGE_NT_HEADERS)
		(pStubDos->e_lfanew + pStubBuf);

	PIMAGE_DOS_HEADER pPeDos = (PIMAGE_DOS_HEADER)m_pFileBuf;
	PIMAGE_NT_HEADERS pPeNt = (PIMAGE_NT_HEADERS)(pPeDos->e_lfanew + m_pFileBuf);

	PIMAGE_TLS_DIRECTORY32 pITD =
		(PIMAGE_TLS_DIRECTORY32)(RvaToOffset(pPeNt->OptionalHeader.DataDirectory[9].VirtualAddress) + m_pFileBuf);

	// 将被加壳程序的 TLS 表指向壳的 TLS表
	pPeNt->OptionalHeader.DataDirectory[9].VirtualAddress =
		(pStubNt->OptionalHeader.DataDirectory[9].VirtualAddress - 0x1000) + NewSectionRva;
	
	// 获取交互数据结构中 TLSIndex 的 Rva
	DWORD IndexRva = ((DWORD)pPackInfo - (DWORD)pStubBuf + 4) - 0x1000 + NewSectionRva + pPeNt->OptionalHeader.ImageBase;
	pITD->AddressOfIndex = IndexRva;
	pITD->StartAddressOfRawData = m_StartOfDataAddress;
	pITD->EndAddressOfRawData = m_EndOfDataAddresss;

	// 然后先取消 TLS 的回调函数, 向交互数据结构体中传入 TLS 回调函数指针, 在壳里边手动调用 TLS回调函数,最后设置回去
	pITD->AddressOfCallBacks = 0;

}



// 函数说明: 加壳的时候把 IAT(导入表) 保存出来
void CPE::SaveImportTab()
{
	if (m_PEImportDir.VirtualAddress == 0)
	{
		return;
	}
	//0.获取导入表结构体指针
	PIMAGE_IMPORT_DESCRIPTOR pPEImport =
		(PIMAGE_IMPORT_DESCRIPTOR)(m_pFileBuf + m_PEImportDir.VirtualAddress);

	//1.第一遍循环确定 m_pModNameBuf 和 m_pFunNameBuf 的大小
	DWORD dwSizeOfModBuf = 0;
	DWORD dwSizeOfFunBuf = 0;
	m_dwNumOfIATFuns = 0;
	while (pPEImport->Name)
	{
		DWORD dwModNameRVA = pPEImport->Name;
		char* pModName = (char*)(m_pFileBuf + dwModNameRVA);
		dwSizeOfModBuf += (strlen(pModName) + 1);

		PIMAGE_THUNK_DATA pIAT = (PIMAGE_THUNK_DATA)(m_pFileBuf + pPEImport->FirstThunk);

		while (pIAT->u1.AddressOfData)
		{
			if (IMAGE_SNAP_BY_ORDINAL(pIAT->u1.Ordinal))
			{
				m_dwNumOfIATFuns++;
			}
			else
			{
				m_dwNumOfIATFuns++;
				DWORD dwFunNameRVA = pIAT->u1.AddressOfData;
				PIMAGE_IMPORT_BY_NAME pstcFunName = (PIMAGE_IMPORT_BY_NAME)(m_pFileBuf + dwFunNameRVA);
				dwSizeOfFunBuf += (strlen(pstcFunName->Name) + 1);
			}
			pIAT++;
		}
		pPEImport++;
	}

	//2.第二遍循环保存信息到自己定义的数据结构里边
	m_pModNameBuf = new CHAR[dwSizeOfModBuf];
	m_pFunNameBuf = new CHAR[dwSizeOfFunBuf];
	m_pMyImport = new MYIMPORT[m_dwNumOfIATFuns];
	memset(m_pModNameBuf, 0, dwSizeOfModBuf);
	memset(m_pFunNameBuf, 0, dwSizeOfFunBuf);
	memset(m_pMyImport, 0, sizeof(MYIMPORT)*m_dwNumOfIATFuns);

	pPEImport =	(PIMAGE_IMPORT_DESCRIPTOR)(m_pFileBuf + m_PEImportDir.VirtualAddress);
	DWORD TempNumOfFuns = 0;
	DWORD TempModRVA = 0;
	DWORD TempFunRVA = 0;
	while (pPEImport->Name)
	{
		DWORD dwModNameRVA = pPEImport->Name;
		char* pModName = (char*)(m_pFileBuf + dwModNameRVA);
		memcpy_s((PCHAR)m_pModNameBuf + TempModRVA, strlen(pModName) + 1, 
			pModName, strlen(pModName) + 1);

		PIMAGE_THUNK_DATA pIAT = (PIMAGE_THUNK_DATA)(m_pFileBuf + pPEImport->FirstThunk);

		while (pIAT->u1.AddressOfData)
		{
			if (IMAGE_SNAP_BY_ORDINAL(pIAT->u1.Ordinal))
			{
				//保存以序号导入方式的函数信息
				m_pMyImport[TempNumOfFuns].m_dwIATAddr = (DWORD)pIAT - (DWORD)m_pFileBuf;
				m_pMyImport[TempNumOfFuns].m_bIsOrdinal = TRUE;
				m_pMyImport[TempNumOfFuns].m_Ordinal = pIAT->u1.Ordinal & 0x7FFFFFFF;
				m_pMyImport[TempNumOfFuns].m_dwModNameRVA = TempModRVA;
			}
			else
			{
				//保存名称导入方式的函数信息
				m_pMyImport[TempNumOfFuns].m_dwIATAddr = (DWORD)pIAT - (DWORD)m_pFileBuf;

				DWORD dwFunNameRVA = pIAT->u1.AddressOfData;
				PIMAGE_IMPORT_BY_NAME pstcFunName = (PIMAGE_IMPORT_BY_NAME)(m_pFileBuf + dwFunNameRVA);
				memcpy_s((PCHAR)m_pFunNameBuf + TempFunRVA, strlen(pstcFunName->Name) + 1,
					pstcFunName->Name, strlen(pstcFunName->Name) + 1);

				m_pMyImport[TempNumOfFuns].m_dwFunNameRVA = TempFunRVA;
				m_pMyImport[TempNumOfFuns].m_dwModNameRVA = TempModRVA;
				TempFunRVA += (strlen(pstcFunName->Name) + 1);
			}
			TempNumOfFuns++;
			pIAT++;
		}
		TempModRVA += (strlen(pModName) + 1);
		pPEImport++;
	}

	//逆序排列 m_pMyImport
	MYIMPORT stcTemp = { 0 };
	DWORD dwTempNum = m_dwNumOfIATFuns / 2;
	for (DWORD i = 0; i < dwTempNum; i++)
	{
		m_pMyImport[i];
		m_pMyImport[m_dwNumOfIATFuns - i - 1];
		memcpy_s(&stcTemp, sizeof(MYIMPORT), &m_pMyImport[i], sizeof(MYIMPORT));
		memcpy_s(&m_pMyImport[i], sizeof(MYIMPORT), &m_pMyImport[m_dwNumOfIATFuns - i - 1], sizeof(MYIMPORT));
		memcpy_s(&m_pMyImport[m_dwNumOfIATFuns - i - 1], sizeof(MYIMPORT), &stcTemp, sizeof(MYIMPORT));
	}

	//保存信息
	m_dwSizeOfModBuf = dwSizeOfModBuf;
	m_dwSizeOfFunBuf = dwSizeOfFunBuf;
}







