#ifndef QUEUE_H
#define QUEUE_H

template<typename T>
class queue {
public:
    virtual bool enqueue(const T&) = 0;
    virtual bool dequeue(T&) = 0;

    virtual void close() = 0;
    virtual bool is_closed() const = 0;
    virtual int size() const = 0;
};

#endif