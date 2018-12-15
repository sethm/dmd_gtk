#[macro_use]
extern crate lazy_static;
extern crate dmd_core;
extern crate libc;

use libc::*;
use std::sync::Mutex;
use std::ptr;
use dmd_core::dmd::Dmd;
use dmd_core::err::DuartError;

lazy_static! {
    static ref DMD: Mutex<Dmd> = Mutex::new(Dmd::new());
}

const SUCCESS: c_int = 0;
const ERROR: c_int = 1;
const BUSY: c_int = 2;

#[no_mangle]
fn dmd_reset() -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            match dmd.reset() {
                Ok(()) => SUCCESS,
                Err(_) => ERROR
            }
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_video_ram() -> *const u8 {
    match DMD.lock() {
        Ok(dmd) => {
            match dmd.video_ram() {
                Ok(video_ram) => video_ram.as_ptr(),
                Err(_) => ptr::null()
            }
        }
        Err(_) => ptr::null()
    }
}

#[no_mangle]
fn dmd_step() -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            match dmd.step_with_error() {
                Ok(()) => SUCCESS,
                Err(_) => ERROR
            }
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_get_pc(pc: &mut uint32_t) -> c_int {
    match DMD.lock() {
        Ok(dmd) => {
            *pc = dmd.get_pc();
            SUCCESS
        },
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_get_duart_output_port(oport: &mut uint8_t) -> c_int {
    match DMD.lock() {
        Ok(dmd) => {
            *oport = dmd.duart_output();
            SUCCESS
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_rx_char(c: uint8_t) -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            match dmd.rx_char(c as u8) {
                Ok(()) => SUCCESS,
                Err(DuartError::ReceiverNotReady) => BUSY
            }
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_rx_keyboard(c: uint8_t) -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            match dmd.rx_keyboard(c) {
                Ok(()) => SUCCESS,
                Err(DuartError::ReceiverNotReady) => BUSY
            }
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_mouse_move(x: uint16_t, y: uint16_t) -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            dmd.mouse_move(x, y);
            SUCCESS
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_mouse_down(button: uint8_t) -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            dmd.mouse_down(button);
            SUCCESS
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn dmd_mouse_up(button: uint8_t) -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            dmd.mouse_up(button as u8);
            SUCCESS
        }
        Err(_) => ERROR
    }
}

#[no_mangle]
fn tx_poll(rx_char: &mut uint8_t) -> c_int {
    match DMD.lock() {
        Ok(mut dmd) => {
            match dmd.tx_poll() {
                Some(c) => {
                    *rx_char = c;
                    SUCCESS
                }
                None => BUSY
            }
        }
        Err(_) => ERROR
    }
}
