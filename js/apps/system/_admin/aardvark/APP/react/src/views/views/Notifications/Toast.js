import Swal from "sweetalert2"
export const Toast = Swal.mixin({
  toast: false,
  position: 'center',
  showConfirmButton: true,
  showCancelButton: true,
  timerProgressBar: false,
  cancelButtonText: "Back",
  confirmButtonText: "Continue",
  confirmButtonColor: "#2ecc71",
  didOpen: (toast) => {
    toast.addEventListener('mouseenter', Swal.stopTimer)
    toast.addEventListener('mouseleave', Swal.resumeTimer)
  }
});


