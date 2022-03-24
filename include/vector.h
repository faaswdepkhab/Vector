#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::move(other.buffer_);
        capacity_ = std::move(other.capacity_);
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::move(rhs.buffer_);
        capacity_ = std::move(rhs.capacity_);
        rhs.buffer_ = nullptr;
        rhs.capacity_ = 0;
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        if (buffer_) {
            Deallocate(buffer_);
        }
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    
    iterator end() noexcept {
        return data_ + size_;
    }
    
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator cend() const noexcept {
        return data_ + size_;
    }
    
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    
    Vector(Vector&& other): 
        data_(std::move(other.data_)),
        size_(std::move(other.size_)) {
            other.size_ = 0;
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    void Swap(Vector &rhs) noexcept {
        std::swap(size_, rhs.size_);
        data_.Swap(rhs.data_);
    }
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                    size_ = rhs.size_;
                } else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress());
                    size_ = rhs.size_;
                }
                
            }
        }
        return *this;
    }
    
    
    Vector& operator=(Vector&& rhs) {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = std::move(rhs.size_);
            rhs.size_ = 0;
        }
        return *this;
    }
    
    template<typename U>
    void PushBack(U &&value) {
        if (size_ == Capacity()) {
            size_t new_size = (size_ == 0) ? 1 : size_ * 2;
            RawMemory<T> new_data(new_size);
            new (new_data + size_) T(std::forward<U>(value));
            OptimalMove(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        } else {
            new (data_ + size_) T(std::forward<U>(value));
        }
        ++size_;
    }
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        } else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }
    
    void PopBack() {
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            size_t new_size = (size_ == 0) ? 1 : size_ * 2;
            RawMemory<T> new_data(new_size);
            new (new_data + size_) T(std::forward<Args>(args)...);
            OptimalMove(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *(data_ + size_ - 1);
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) {
            EmplaceBack(std::forward<Args>(args)...);
            return begin() + size_ - 1;
        }
        
        size_t index = pos - begin();
        if (size_ < Capacity()) {
            T t = T(std::forward<Args>(args)...);
            std::uninitialized_move_n(end() - 1, 1, end());
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(t);
        } else {
            size_t new_size = (size_ == 0) ? 1 : size_ * 2;
            RawMemory<T> new_data(new_size);
            new (new_data + index) T(std::forward<Args>(args)...);
            try {
                OptimalMove(data_.GetAddress(), index, new_data.GetAddress());
            } catch (...) {
                std::destroy_n(new_data + index, 1);
            }
            try {
                OptimalMove(data_ + index, size_ - index, new_data + index + 1);
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), index + 1);
            }
            data_.Swap(new_data);
        }
        ++size_;
        return begin() + index;
    }
   
    iterator Erase(const_iterator pos) {
        if (size_ == 0) {
            return begin();
        }
        size_t index = pos - begin();
        std::move(begin() + index + 1, end(), begin() + index);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + index;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
    
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
    
    void OptimalMove(T *src, size_t count, T *dest) {
        // если move-конструктор не выбрасывает исключений или нет copу-конструктора,
        // то делаем перемещение, иначе копируем элементы из старой области памяти в новую
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(src, count, dest);
        } else {
            std::uninitialized_copy_n(src, count, dest);
        }
        std::destroy_n(src, count);
    }
};
