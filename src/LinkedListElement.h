#pragma once

#include "platglue.h"
#include <stdio.h>

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

    void AddToList(LinkedListElement* newElement)
    {
        // add to the end of list
	newElement->m_Prev = m_Prev;
	newElement->m_Next = this;
	m_Prev->m_Next = newElement;
	m_Prev = newElement;
        printf("LinkedListElement::AddToList (%p)->(%p)->(%p)\n", m_Prev, this, m_Next);
    }
    
    int NotEmpty(void)
    {
        return (m_Next != this);
    }
    
    ~LinkedListElement()
    {
       if (m_Next != this && m_Prev != this)
       {
           printf("~LinkedListElement(%p)->(%p)->(%p)\n", m_Prev, this, m_Next);
           m_Next->m_Prev = m_Prev;
           m_Prev->m_Next = m_Next;
           printf("~LinkedListElement after: (%p)->(%p)", m_Prev, m_Prev->m_Next);
       }
    }
};
