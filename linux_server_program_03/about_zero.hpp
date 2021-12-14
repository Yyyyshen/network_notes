//Copyright(c) by yyyyshen 2021

#ifndef ABOUT_ZERO_HPP
#define ABOUT_ZERO_HPP

//
//关于C中的各种0
// NULL、0、'0'、'\0'、"0"、"\0"
//

#include <iostream>

void
test_NULL()
{
	int zero = 0;//0

	int a = NULL; //0，cpp中为0

	void* p = NULL;//0, C中的定义为 (void*)0；而C++中空指针应使用nullptr
}

void
test_char_0()
{
	char a = '\0';//0，用字符表示实际值为0的数字，需要转移
	char b = '0';//48，字符0在ascii上是48

	//所以，在内存清零时
	char* buf = new char[1024];
	memset(buf, 0, 1024);
	memset(buf, '\0', 1024);//两个一样
	//但这样就不同了，内存里会被填充48
	memset(buf, '0', 1024);
}

void
test_str_0()
{
	const char* str1 = "\0";//数组长度为2，两个字节都是空字符
	int str1_len = strlen(str1);//判断字符串结尾用'\0'，所以结果是0

	const char* str2 = "0";//数组长度2，str2[0]是0，str[1]是'\0'，strlen为1

	//只要是双引号，结尾都默认带有一个'\0'
}

#endif // !ABOUT_ZERO_HPP