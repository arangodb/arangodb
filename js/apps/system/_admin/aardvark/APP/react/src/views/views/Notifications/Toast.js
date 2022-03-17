import React from "react"
import Swal from "sweetalert2"
export const Toast = Swal.mixin({
  toast: false,
  position: 'top-end',
  showConfirmButton: true,
  timer: 3000,
  timerProgressBar: false,
  confirmButtonText: "Continue",
  didOpen: (toast) => {
    toast.addEventListener('mouseenter', Swal.stopTimer)
    toast.addEventListener('mouseleave', Swal.resumeTimer)
  }
});


