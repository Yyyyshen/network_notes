//Copyright(c) by yyyyshen 2021

#ifndef ABOUT_ZERO_HPP
#define ABOUT_ZERO_HPP

//
//����C�еĸ���0
// NULL��0��'0'��'\0'��"0"��"\0"
//

#include <iostream>

void
test_NULL()
{
	int zero = 0;//0

	int a = NULL; //0��cpp��Ϊ0

	void* p = NULL;//0, C�еĶ���Ϊ (void*)0����C++�п�ָ��Ӧʹ��nullptr
}

void
test_char_0()
{
	char a = '\0';//0�����ַ���ʾʵ��ֵΪ0�����֣���Ҫת��
	char b = '0';//48���ַ�0��ascii����48

	//���ԣ����ڴ�����ʱ
	char* buf = new char[1024];
	memset(buf, 0, 1024);
	memset(buf, '\0', 1024);//����һ��
	//�������Ͳ�ͬ�ˣ��ڴ���ᱻ���48
	memset(buf, '0', 1024);
}

void
test_str_0()
{
	const char* str1 = "\0";//���鳤��Ϊ2�������ֽڶ��ǿ��ַ�
	int str1_len = strlen(str1);//�ж��ַ�����β��'\0'�����Խ����0

	const char* str2 = "0";//���鳤��2��str2[0]��0��str[1]��'\0'��strlenΪ1

	//ֻҪ��˫���ţ���β��Ĭ�ϴ���һ��'\0'
}

#endif // !ABOUT_ZERO_HPP