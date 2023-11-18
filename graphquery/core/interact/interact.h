#pragma once

namespace graphquery::interact
{
    /****************************************************************
    ** \class IInteract
    ** \brief Interact class that is connected towards the database
    **        which provides a method of interacting with the system.
    ***************************************************************/
    class IInteract
    {
    public:
        /****************************************************************
        ** \brief Creates an instance of the class (for derived classes).
        ***************************************************************/
        explicit IInteract() = default;
        /****************************************************************
        ** \brief Destructs an instance of the class (for derived classes).
        ***************************************************************/
        virtual ~IInteract() = default;

        /****************************************************************
        * \brief Virtual render function for the database to call
        *        to provide an interaction for the user to the system.
        *
         * \return void
        **************************************************************/
        virtual void Render() noexcept = 0;
        virtual void Clean_Up() {};
    };
}