#include "contiguous_block.h"

ContiguousBlock::ContiguousBlock()   // Constructor
{
    bInit = true;
    bModified = true;
    starting_seq_number = 0;
    next_expected_seq_number = 0;
    p_s_contiguous_block = new string("");
}

ContiguousBlock::ContiguousBlock(const ContiguousBlock &copyin)   // Copy constructor to handle pass by value.
{
    starting_seq_number = copyin.starting_seq_number;
    next_expected_seq_number = copyin.next_expected_seq_number;
    p_s_contiguous_block = new string(*(copyin.p_s_contiguous_block));
    bInit = copyin.bInit;
    bModified = copyin.bModified;
}

ostream &operator<<(ostream &output, const ContiguousBlock &cb)
{
    output << cb.starting_seq_number << " " << cb.next_expected_seq_number << " yay" << endl;
    return output;
}

ContiguousBlock& ContiguousBlock::operator=(const ContiguousBlock &rhs)
{
    this->starting_seq_number = rhs.starting_seq_number;
    this->next_expected_seq_number = rhs.next_expected_seq_number;
    return *this;
}

int ContiguousBlock::operator==(const ContiguousBlock &rhs) const
{
    if ( (this->starting_seq_number == rhs.starting_seq_number) && (this->next_expected_seq_number == rhs.next_expected_seq_number) ) return 1;
    return 0;
}

// This function is required for built-in STL list functions like sort
int ContiguousBlock::operator<(const ContiguousBlock &rhs) const
{
   if( this->next_expected_seq_number < rhs.starting_seq_number ) return 1;
   return 0;
}

void ContiguousBlock::append(const char* str, const int size_payload, const tcp_seq start_seq_number)
{
    if (bInit) {
        starting_seq_number = next_expected_seq_number = start_seq_number;
        bInit = false;
    }

    // giving the size_payload param here makes sure that we append \0s too from the char*
    p_s_contiguous_block->append(str, size_payload);
    next_expected_seq_number  += size_payload;
    //cout << "next_expected_seq_number : " << next_expected_seq_number << endl;
    bModified = true;
}

// used while merging the next ContiguousBlock with the current one in the list
void ContiguousBlock::append(const string* str, const tcp_seq end_seq_number)
{
    // we don't need the size_payload param as we are using a string and not a char*,
    // hence the \0s will be copied
    p_s_contiguous_block->append(*str);
    next_expected_seq_number = end_seq_number;
    bModified = true;
}

// used while merging the next ContiguousBlock with the current one in the list
void ContiguousBlock::overlapping_append(const string* str, const unsigned int offset, const tcp_seq end_seq_number)
{
    // we don't need the size_payload param as we are using a string and not a char*,
    // hence the \0s will be copied
    p_s_contiguous_block->append(*str, offset, str->size() - offset);
    next_expected_seq_number = end_seq_number;
    bModified = true;
}


void ContiguousBlock::prepend(const char* str, const int size_payload, const tcp_seq start_seq_number)
{
    starting_seq_number = start_seq_number;

    //string* new_str = new string(str);

    // giving the size_payload param here makes sure that we append \0s too from the char*
    string* new_str = new string();
    new_str->append(str, size_payload);

    new_str->append(*p_s_contiguous_block);
    delete p_s_contiguous_block;
    p_s_contiguous_block = new_str;
    bModified = true;
}

/*
int main()
{
   list<ContiguousBlock> L;
   ContiguousBlock cb;

   cb.last_seq_number = 10;
   L.push_back(cb);  // Insert a new element at the end

   cb.last_seq_number = 3;
   L.push_back(cb);  // Object passed by value. Uses default member-wise copy constructor

   cb.last_seq_number = 5;
   L.push_back(cb);

   list<ContiguousBlock>::iterator i;

   for(i=L.begin(); i != L.end(); ++i) cout << (*i).last_seq_number << " "; // print member
   cout << endl;
   for(i=L.begin(); i != L.end(); ++i) cout << *i << " "; // print all
   cout << endl;

   cout << "Sorted: " << endl;
   L.sort();
   for(i=L.begin(); i != L.end(); ++i) cout << *i << " "; // print all
   cout << endl;

   return 0;
    list<int> l;
    l.push_back(1);
    l.push_back(2);
    l.push_back(3);
    l.push_back(4);


    list<int>::iterator i;
    for (i = l.begin(); i != l.end(); ++i) {
        if (*i == 3) {
            l.insert(i, 5);
        }
    }

    for(i=l.begin(); i != l.end(); ++i) {

        if (*i == 5) {
            list<int>::iterator i2(i);
            i2++;
            l.erase(i2);
        }
        cout << *i << " "; // print all
    }
    cout << endl;
}
*/
