#pragma once



#include "Engine/Core/Log/Result.h"



#include <memory>

#include <string>

#include <string_view>



template<typename T>

Result<std::unique_ptr<T>> FailResourceCreation(std::string message)

{

    return MakeFail<std::unique_ptr<T>>(

        ErrorCode::ResourceCreationFailed,

        std::move(message));

}



template<typename T>

Result<std::unique_ptr<T>> MakeResourceResult(

    std::unique_ptr<T> resource,

    std::string_view message)

{

    if (resource && resource->IsValid())

    {

        return MakeOk(std::move(resource));

    }



    return FailResourceCreation<T>(std::string(message));

}

template<typename Base, typename Derived>
Result<std::unique_ptr<Base>> CastResourceResult(Result<std::unique_ptr<Derived>> result)
{
    if (!result)
    {
        return MakeFail<std::unique_ptr<Base>>(result.error.code, std::move(result.error.message));
    }
    return MakeOk(std::unique_ptr<Base>(result.value.release()));
}

