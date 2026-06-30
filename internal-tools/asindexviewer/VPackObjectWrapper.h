#pragma once

template <typename T>
class VPackObjectSetProperty {
public:
    VPackObjectSetProperty(T& obj, const std::string& fieldName)
        : _obj(obj), _fieldName(fieldName) {}

    void operator =(const VPackValue& value) {
        _obj.add(_fieldName, value);
    }

private:
    const std::string& _fieldName;
    T& _obj;
};

template <typename T>
class VPackObjectWrapperBase {
public:
    VPackObjectWrapperBase(T& obj) : _obj(obj) {
    };

    ~VPackObjectWrapperBase() {
        _obj.close();
    }

    VPackObjectSetProperty<T> operator [](const std::string& fieldName) {
        VPackObjectSetProperty<T> setProp(_obj, fieldName);
        return std::move(setProp);
    }

    T& get() {
        return _obj;
    }

protected:
    T& _obj;
};

template <typename T>
class VPackObjectWrapper : public VPackObjectWrapperBase<T> {
public:
    VPackObjectWrapper(T& obj) : VPackObjectWrapperBase<T>(obj) {
        this->_obj.add(VPackValue(VPackValueType::Object));
    }

    VPackObjectWrapper(T& obj, const std::string& fieldName) : VPackObjectWrapperBase<T>(obj) {
        this->_obj.add(fieldName, VPackValue(VPackValueType::Object));
    }
};

template <typename T>
class VPackArrayWrapper : public VPackObjectWrapperBase<T> {
public:
    VPackArrayWrapper(T& obj, const std::string& fieldName) : VPackObjectWrapperBase<T>(obj) {
        this->_obj.add(fieldName, VPackValue(VPackValueType::Array));
    }
};
