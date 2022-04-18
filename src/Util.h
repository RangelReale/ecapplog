#pragma once

#include <cstdint>
#include <cstdlib>
#include <chrono>

namespace Util
{

namespace detail {
    template <size_t N>
    class circular_buffer
    {
    public:
        bool advance(int count)
        {
            if (count <= 0) return false;
            if (count >= N) count = N;
            int startIndex = index_ + 1;
            for (int clnindex = startIndex; clnindex < startIndex + count; clnindex++)
            {
                index_ = clnindex % N;
                samples_[index_] = 0;
                if (num_samples_ < N) num_samples_++;
            }
            return true;
        }

        void add(int value)
        {
            samples_[index_] += value;
            if (num_samples_ == 0) num_samples_ = 1;
        }

        int sum()
        {
            int ret = 0;
            for (int i = 0; i < num_samples_; i++)
            {
                ret += samples_[i];
            }
            return ret;
        }

        double avg() const
        {
            if (num_samples_ < 2) return 0;

            int total = 0;
            for (int i = 0; i < num_samples_; i++)
            {
                if (i == index_) continue; // current item is still in progress
                total += samples_[i];
            }
            return static_cast<double>(total) / static_cast<double>(num_samples_ - 1);
        }
    private:
        int index_{ 0 };
        size_t num_samples_{ 0 };
        int samples_[N]{ 0 };
    };
}

template <size_t N>
class ItemPerSecond
{
protected:
    virtual int32_t elapsed() const = 0;
    virtual void elapsedReset(int32_t remain_ms) = 0;
public:
    bool sample(int amount)
    {
        auto [curamount, r] = std::div(elapsed(), 1000);
        bool ret = samples_.advance(curamount);
        if (ret) {
            elapsedReset(r);          
        }
        samples_.add(amount);
        return ret;
    }

    double avg() const
    {
        return samples_.avg();
    }
private:
    detail::circular_buffer<N> samples_;
};

template <size_t N>
class ItemPerSecondChrono : public ItemPerSecond<N>
{
public:
    ItemPerSecondChrono()
    {
        last_ = std::chrono::steady_clock::now();
    }
protected:
    int32_t elapsed() const override
    {
        return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_).count());
    }

    void elapsedReset(int32_t remain_ms) override
    {
        last_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(remain_ms);
    }
private:
    std::chrono::steady_clock::time_point last_;
};

template <size_t N>
class ItemPerSecondTest : public ItemPerSecond<N>
{
protected:
    int32_t elapsed() const override
    {
        return elapsed_;
    }
    void elapsedReset(int32_t remain_ms) override
    {
        elapsed_ = remain_ms;
    }
public:
    bool sampleTest(int amount, int32_t elapsed)
    {
        elapsed_ += elapsed;
        return ItemPerSecond<N>::sample(amount);
    }
private:
    int32_t elapsed_{ 0 };
};

}
