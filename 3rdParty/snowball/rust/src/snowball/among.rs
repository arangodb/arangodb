use snowball::SnowballEnv;

pub struct Among<T: 'static>(pub &'static str,
                             pub i32,
                             pub i32,
                             pub Option<&'static (dyn Fn(&mut SnowballEnv, &mut T) -> bool + Sync)>);
