#ifndef _R_Mes_queue_h
#define _R_Mes_queue_h

#include "types.h"

class R_Message;

class R_Mes_queue
{
   // 
   // Implements a bounded queue of messages.
   //
public:
   R_Mes_queue();
   // Effects: Creates an empty queue that can hold one request per principal.

   bool append(R_Message *r);
   // Effects: If there is space in the queue and there is no request
   // from "r->client_id()" with timestamp greater than or equal to
   // "r"'s in the queue then it: appends "r" to the queue, removes any
   // other request from "r->client_id()" from the queue and returns
   // true. Otherwise, returns false.

   R_Message *remove();
   // Effects: If there is any element in the queue, removes the first
   // element in the queue and returns it. Otherwise, returns 0.

   R_Message* first() const;
   // Effects: If there is any element in the queue, returns a pointer to
   // the first request in the queue. Otherwise, returns 0.

   int size() const;
   // Effects: Returns the current size (number of elements) in queue.

   // Effects: Returns true if there are no elements in the queue.
   bool empty() const;

   int num_bytes() const;
   // Effects: Return the number of bytes used by elements in the queue.

private:
   struct PR_Node
   {
      R_Message* m;
      PR_Node* next;
      PR_Node* prev;

      PR_Node(R_Message *msg)
      {
         m = msg;
      }
      ~PR_Node()
      {

      }
   };

   PR_Node* head;
   PR_Node* tail;

   int nelems; // Number of elements in queue
   int nbytes; // Number of bytes in queue

};

inline int R_Mes_queue::size() const
{
   return nelems;
}

inline int R_Mes_queue::num_bytes() const
{
   return nbytes;
}

inline R_Message *R_Mes_queue::first() const
{
   if (head)
   {
      //th_assert(head->m != 0, "Invalid state");
      return head->m;
   }
   return 0;
}

inline bool R_Mes_queue::empty() const
{
    return nelems == 0;
}

#endif // _R_Mes_queue_h
