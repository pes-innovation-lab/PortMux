#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Protocol {
    pub name: &'static str,
    pub port: u16,
    pub priority: String,
}
