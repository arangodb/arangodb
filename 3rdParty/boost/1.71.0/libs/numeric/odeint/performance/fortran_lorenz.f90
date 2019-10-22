program main
    implicit none

    integer, parameter :: dp = 8
    real(dp), dimension(1:3) :: x
    integer, parameter :: nstep = 20000000
    real(dp) :: t = 0.0_dp
    real(dp) :: h = 1.0e-10_dp
    integer, parameter :: nb_loops = 21
    integer, parameter :: n = 3
    integer :: k
    integer :: time_begin
    integer :: time_end
    integer :: count_rate
    real(dp) :: time
    real(dp) :: min_time = 100.0

    do k = 1, nb_loops
        x = [ 8.5_dp, 3.1_dp, 1.2_dp ]
        call system_clock(time_begin, count_rate)
        call rk4sys(n, t, x, h, nstep)
        call system_clock(time_end, count_rate)
        time = real(time_end - time_begin, dp) / real(count_rate, dp)
        min_time = min(time, min_time)
        write (*,*) time, x(1)
    end do
    write (*,*) "Minimal Runtime:", min_time
contains
    subroutine xpsys(x,f)
        real(dp), dimension(1:3), intent(in) :: x
        real(dp), dimension(1:3), intent(out) :: f
        f(1) = 10.0_dp * ( x(2) - x(1) )
        f(2) = 28.0_dp * x(1) - x(2) - x(1) * x(3)
        f(3) = x(1) * x(2) - (8.0_dp / 3.0_dp) * x(3)
    end subroutine xpsys

    subroutine rk4sys(n, t, x, h, nstep)
        integer, intent(in) :: n
        real(dp), intent(in) :: t
        real(dp), dimension(1:n), intent(inout) :: x
        real(dp), intent(in) :: h
        integer, intent(in) :: nstep
        ! Local variables
        real(dp) :: h2
        real(dp), dimension(1:n) :: y, f1, f2, f3, f4
        integer :: i, k

        h2 = 0.5_dp * h
        do k = 1, nstep
            call xpsys(x, f1)
            y = x + h2 * f1
            call xpsys(y, f2)
            y = x + h2 * f2
            call xpsys(y, f3)
            y = x + h * f3
            call xpsys(y, f4)
            x = x + h * (f1 + 2.0_dp * (f2 + f3) + f4) / 6.0_dp
        end do
    end subroutine rk4sys
end program main
