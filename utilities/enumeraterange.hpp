#pragma once

#include <cstddef>
#include <utility>
#include <iterator>
#include <optional>

template<typename Iterator, typename CounterType = std::uint32_t>
struct EnumerateIterator
{
    using iterator_category = typename Iterator::iterator_category;
    using value_type        = std::tuple<CounterType, typename Iterator::value_type>;
    using difference_type   = typename Iterator::difference_type;
    using pointer           = value_type*;
    using reference         = value_type&;

    Iterator                  mIterator;
    CounterType               mCounter;
    // TODO(andrea.machizaud) we may relax this requirement dependending on the iterator category/user input
    // to satisfy operator* which returns a reference, we need to fetch data from iterator, however we delay this operatiom
    mutable std::optional<value_type> mData;

    EnumerateIterator(Iterator iterator, CounterType start = {})
        : mIterator(iterator)
        , mCounter(start)
    {
    }

    EnumerateIterator(const EnumerateIterator& rhs)
        : mIterator(rhs.mIterator)
        , mCounter(rhs.mCounter)
    {
    }

    EnumerateIterator(EnumerateIterator&& rhs)
        : mIterator(std::move(rhs.mIterator))
        , mCounter(std::move(rhs.mCounter))
    {
    }

    EnumerateIterator& operator=(const EnumerateIterator& rhs)
    {
        mIterator = rhs.mIterator;
        mCounter = rhs.mCounter;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator& operator=(EnumerateIterator&& rhs)
    {
        mIterator = std::move(rhs.mIterator);
        mCounter = std::move(rhs.mCounter);
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator& operator++()
    {
        ++mIterator;
        ++mCounter;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator operator++(int) const
    {
        return EnumerateIterator(mIterator + 1, mCounter + 1);
    }

    EnumerateIterator& operator+=(difference_type count)
    {
        mIterator += count;
        mCounter += count;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator& operator--()
    {
        --mIterator;
        --mCounter;
        reset_data_fetch();
        return *this;
    }

    EnumerateIterator operator--(int) const
    {
        return EnumerateIterator(mIterator - 1, mCounter - 1);
    }

    EnumerateIterator& operator-=(difference_type count)
    {
        mIterator -= count;
        mCounter -= count;
        reset_data_fetch();
        return *this;
    }

    void reset_data_fetch()
    {
        mData.reset();
    }

    void ensure_data_has_been_fetched() const
    {
        if (!mData.has_value())
            mData = std::make_tuple(mCounter, *mIterator);
    }

    reference operator*() const
    {
        ensure_data_has_been_fetched();
        return *mData;
    }
};

template<typename Iterator, typename CounterType>
bool operator==(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator == rhs.mIterator;
}

template<typename Iterator, typename CounterType>
bool operator!=(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return !(lhs == rhs);
}

template<typename Iterator, typename CounterType>
typename EnumerateIterator<Iterator, CounterType>::difference_type operator+(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator + rhs.mIterator;
}

template<typename Iterator, typename CounterType>
typename EnumerateIterator<Iterator, CounterType>::difference_type operator-(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator - rhs.mIterator;
}

template<typename Iterator, typename CounterType>
auto operator<=>(const EnumerateIterator<Iterator, CounterType>& lhs, const EnumerateIterator<Iterator, CounterType>& rhs)
{
    return lhs.mIterator <=> rhs.mIterator;
}

template<typename Container, typename CounterType = std::uint32_t>
struct EnumerateRange
{
    using Iterator = EnumerateIterator<typename Container::iterator, CounterType>;

    Iterator mStart, mEnd;

    explicit EnumerateRange(Container& container, CounterType start = {})
        : mStart(std::begin(container), start)
        , mEnd(std::end(container), {})
    {
    }

    Iterator begin() { return mStart; }
    const Iterator cbegin() const { return mStart; }
    Iterator end() { return mEnd; }
    const Iterator cend() const { return mEnd; }
};

template<typename Container, typename CounterType = std::uint32_t>
struct ConstEnumerateRange
{
    using Iterator = EnumerateIterator<typename Container::const_iterator, CounterType>;

    Iterator mStart, mEnd;

    explicit ConstEnumerateRange(const Container& container, CounterType start = {})
        : mStart(std::begin(container), start)
        , mEnd(std::end(container), {})
    {
    }

    Iterator begin() { return mStart; }
    const Iterator cbegin() const { return mStart; }
    Iterator end() { return mEnd; }
    const Iterator cend() const { return mEnd; }
};

template<typename Container>
auto enumerate(Container& container)
{
    return EnumerateRange(container);
}

template<typename Container>
auto enumerate(const Container& container)
{
    return ConstEnumerateRange(container);
}
