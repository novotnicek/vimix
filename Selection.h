#ifndef SELECTION_H
#define SELECTION_H

#include "Source.h"

class Selection
{

public:
    Selection();

    // construct list
    void add    (Source *s);
    void add    (SourceList l);
    void remove (Source *s);
    void remove (SourceList l);
    void set    (Source *s);
    void set    (SourceList l);
    void toggle (Source *s);

    void clear  ();
    void pop_front();

    // access elements
    SourceList::iterator begin ();
    SourceList::iterator end ();
    Source *front();
    Source *back();

    // properties
    bool contains (Source *s);
    bool empty();
    uint size ();

    // extract
    std::string xml();
    SourceList depthSortedList();

protected:
    SourceList::iterator find (Source *s);
    SourceList selection_;
};

#endif // SELECTION_H
