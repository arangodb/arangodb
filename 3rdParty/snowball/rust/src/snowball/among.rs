use snowball::SnowballEnv;

pub struct Among<T: 'static>(pub &'static str,
                             pub i32,
                             pub i32,
                             pub Option<&'static (Fn(&mut SnowballEnv, &mut T) -> bool + Sync)>);
