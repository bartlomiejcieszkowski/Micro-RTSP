#pragma once

#include "platglue.h"

class LinkedListElement
{
public:
	LinkedListElement* m_Next;
	LinkedListElement* m_Prev;
	
	LinkedListElement(void)
	{
		m_Next = this;
		m_Prev = this;
	}
	
	int IsEmpty(void)
	{
		return (m_Next == this);
	}
	
	LinkedListElement(LinkedListElement* linkedList)
	{
		// add to the end of list
		m_Prev = linkedList->m_Prev;
		linkedList->m_Prev = this;
		m_Prev->m_Next = this;
	}
	
	~LinkedListElement()
	{
		if (m_Next)
			m_Next->m_Prev = m_Prev;
		if (m_Prev)
			m_Prev->m_Next = m_Next;
	}
};
