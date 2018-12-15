#[no_mangle]
fn test_function() -> u8 {
    return 0x5a;
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
