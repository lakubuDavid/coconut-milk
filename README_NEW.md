# Coconut Milk - System Architecture Documentation

## Overview

Coconut Milk is a **task-based collaboration framework** with a Lua runtime that enables **visual programming, workflow orchestration, and multi-agent coordination**. The system is designed for flexible automation with role-based access control and cross-language support.

## System Architecture

### Core Components

#### 1. WebView Runtime
- **Type**: Core browser-based component
- **Purpose**: Primary execution environment for workflows
- **Features**:
  - Lambda-style function execution
  - Web interface integration
  - Cross-platform UI rendering
  - Event-driven architecture

#### 2. Worker Processes
- **Types**:
  - **Main Process**: Entry point with async tasks
  - **Parallel Workers**: Concurrent execution
  - **Sequential Workers**: Ordered task processing
  - **Bridge Workers**: Communication gateways
- **Capabilities**:
  - Task scheduling and execution
  - Role-based permissions
  - Error handling and recovery
  - Logging and monitoring

#### 3. Bridge System
- **Types**:
  - **Background Bridge**: Internal communication
  - **Primary Bridge**: External integrations
  - **Sync Bridge**: State synchronization
- **Functions**:
  - RPC (Remote Procedure Call) communications
  - Message queuing
  - Event routing
  - Error propagation

#### 4. Configuration Management
- **Engine**: Dynamic configuration system
- **Formats**: YAML, JSON, Lua tables
- **Features**:
  - Runtime configuration updates
  - Environment-specific settings
  - Permission management
  - Role-based access control

### Tech Stack

#### Runtime Layer
- **Lua**: Core runtime language with C++ bindings
- **C++**: Performance-critical components
- **xmake**: Build system
- **cargo**: Rust integration (when needed)

#### Frontend Layer
- **WebView**: Browser-based UI
- **JavaScript/TypeScript**: Frontend logic
- **HTML/CSS**: UI components
- **CSS frameworks**: Visual styling

#### Workflow Languages
- **Lua**: Primary workflow scripting
- **Workflow definitions**: Structured YAML/JSON formats
- **DSL**: Domain-specific language for workflow composition

### Modules Diagram

```d2
模型: 组件
    方向: right
    样式: rounded

    组件: 组件
    模块 A: 模块
    模块 B: 模块
    模块 C: 模块

    组件 -.> 模块 A
    组件 -.> 模块 B
    组件 -.> 模块 C

模型: 数据流
    方向: right
    样式: rounded

    模块 A -.> 模块 B
    模块 B -.> 模块 C
    模块 C -.> 数据库
```

### Worker Pool and Dispatcher

#### Worker Architecture
```d2
模型: 工人池
    方向: right
    样式: rounded

    调度器: 调度器
    工作者 1: 工作者
    工作者 2: 工作者
    工作者 3: 工作者

    调度器 -.> 工作者 1
    调度器 -.> 工作者 2
    调度器 -.> 工作者 3

模型: 主运行时
    方向: right
    样式: rounded

    调度器 -.> 主运行时
    主运行时 -.> 任务
    主运行时 -.> 数据库
    主运行时 -.> 日志
```

#### Communication Flow
```d2
模型: 通信流
    方向: right
    样式: rounded

    客户端: 客户端
    API 网关: API 网关
    服务: 服务
    数据库: 数据库

    客户端 -.> API 网关
    API 网关 -.> 服务
    服务 -.> 数据库
    服务 -.> 客户端
```

### Project Architecture

#### Directory Structure
```
/coconut-milk
├── apps/
│   ├── coconut/                    # Main application
│   │   ├── src/                   # Source code
│   │   │   ├── core/              # Core modules
│   │   │   ├── modules/           # Plugin modules
│   │   │   └── platform/          # Platform-specific code
│   │   └── res/                    # Resources
│   └── coconut-cli/                # Command line interface
│       └── src/                   # CLI source
├── docs/                           # Documentation
│   ├── architecture/              # Architecture docs
│   └── api-reference/             # API documentation
├── examples/                       # Usage examples
├── packages/                       # Package definitions
└── tests/                          # Test suites
```

## Diagrams

### Sequence Diagrams

#### Command Processing Flow
```d2
sequence: Command Processing
    participant: App
    participant: Bridge
    participant: Worker
    participant: Database

    App -> Bridge: Submit command
    Bridge -> Worker: Execute task
    Worker -> Database: Access data
    Worker -> App: Return result
    App -> Bridge: Acknowledge
```

#### Event Dispatch Chain
```d2
sequence: Event Processing
    participant: EventSource
    participant: Dispatcher
    participant: Worker1
    participant: Worker2
    participant: Database

    EventSource -> Dispatcher: Emit event
    Dispatcher -> Worker1: Route event
    Dispatcher -> Worker2: Route event
    Worker1 -> Database: Store event
    Worker2 -> Database: Process event
    Worker2 -> Dispatcher: Event complete
```

#### WebView Lifecycle
```d2
sequence: WebView Lifecycle
    participant: Main
    participant: WebView
    participant: Loader
    participant: Script

    Main -> WebView: Initialize
    WebView -> Loader: Load HTML/JS
    Loader -> Script: Execute
    Script -> WebView: UI update
    WebView -> Main: Ready state
```

### Architecture Diagrams

#### System Relationships
```d2
模型: 系统关系
    方向: right
    样式: rounded

    应用层: 应用层
    业务逻辑层: 业务逻辑层
    数据访问层: 数据访问层
    基础设施层: 基础设施层

    应用层 -.> 业务逻辑层
    业务逻辑层 -.> 数据访问层
    数据访问层 -.> 基础设施层
```

#### Runtime Flow
```d2
模型: 运行时流程
    方向: right
    样式: rounded

    客户端: 客户端
    网关: 网关
    服务: 服务
    队列: 队列
    数据库: 数据库

    客户端 -.> 网关
    网关 -.> 服务
    服务 -.> 队列
    服务 -.> 数据库
    服务 -.> 客户端
```

## Technology Stack Analysis

### 1. Core Runtime
- **Lua**: Primary scripting language with FFI (Foreign Function Interface)
- **C++**: Performance-critical components
- **xmake**: Build and dependency management
- **cargo**: Rust-based tools integration

### 2. Frontend Components
- **WebView**: Cross-platform browser component
- **JavaScript/TypeScript**: Frontend logic and interactivity
- **HTML/CSS**: UI framework and styling
- **Node.js**: Development tooling

### 3. Communication Protocols
- **RPC**: Remote procedure calls for inter-process communication
- **Message Queues**: Async task processing
- **WebSockets**: Real-time communication
- **HTTP APIs**: External service integration

### 4. Development Tools
- **xmake**: Modern build system
- **cargo**: Rust package manager
- **Node Package Manager**: Frontend dependencies
- **Git**: Version control

## Icons and UI Components

### Icon Usage
- **SVG Icons**: Scalable vector graphics for UI elements
- **lucide**: Icon library integration
- **Custom SVG**: Brand-specific icons
- **Component-based**: Icon systems with themes

### Icon Library Integration
```lua
-- Icon configuration example
icons = {
    home = "lucide/home",
    settings = "lucide/settings", 
    user = "lucide/user",
    workflow = "lucide/workflow",
    branch = "lucide/git-branch",
    merge = "lucide/git-merge"
}
```

## Supported Diagrams and Visualizations

### 1. Workflow Diagrams
- **Flowcharts**: Process flow and decision trees
- **Sequence Diagrams**: Event ordering and timing
- **State Machines**: Application state transitions

### 2. Architecture Diagrams
- **Component Relationships**: System module interactions
- **Data Flow**: Information flow between components
- **Deployment Diagrams**: Infrastructure architecture

### 3. Process Diagrams
- **Workflow Maps**: Business process visualization
- **Sequence Charts**: Task execution order
- **Interaction Diagrams**: Component communication

## Examples and Templates

### 1. Basic Workflow
```lua
workflow "simple-workflow" {
    step "input" {
        type = "input"
        description = "Get user input"
    }
    
    step "process" {
        type = "function"
        function = "process_data"
        depends_on = ["input"]
    }
    
    step "output" {
        type = "output"
        depends_on = ["process"]
    }
}
```

### 2. Advanced Workflow
```lua
workflow "complex-workflow" {
    parallel {
        step "data-extraction" {
            type = "external-api"
            service = "weather-service"
        }
        
        step "data-processing" {
            type = "function"
            function = "analyze-data"
        }
    }
    
    step "aggregation" {
        type = "function"
        function = "generate-report"
        depends_on = ["data-extraction", "data-processing"]
    }
    
    step "notification" {
        type = "notification"
        channel = "slack"
        depends_on = ["aggregation"]
    }
}
```

## Implementation Guide

### Step 1: Project Setup
```bash
# Clone the repository
git clone https://github.com/lakubuDavid/coconut-milk.git
cd coconut-milk

# Install dependencies
xmake install

# Set up development environment
node install
npm install
```

### Step 2: Create First Workflow
```lua
-- Create a new workflow file
create workflow "getting-started"

-- Add steps
workflow.add_step "start", type = "function", function = "hello_world"
workflow.add_step "process", type = "external-api", service = "weather"
workflow.add_step "end", type = "output"

-- Execute workflow
workflow.run "getting-started"
```

### Step 3: Add UI Components
```html
<!-- Example component with icons -->
<div class="workflow-step">
    <lucide-icon name="workflow" size="24" />
    <span class="step-label">Data Processing</span>
    <div class="step-content">
        <!-- Step content here -->
    </div>
</div>
```

## Best Practices

### 1. Architecture Design
- **Modular Design**: Keep components loosely coupled
- **Separation of Concerns**: Distinct layers for different responsibilities
- **Scalability**: Design for horizontal and vertical scaling
- **Maintainability**: Clear documentation and code organization

### 2. Performance Optimization
- **Async Processing**: Use non-blocking I/O operations
- **Caching**: Implement appropriate caching strategies
- **Resource Management**: Monitor and optimize resource usage
- **Load Balancing**: Distribute workload across available resources

### 3. Security Considerations
- **Access Control**: Implement role-based access control
- **Encryption**: Secure sensitive data in transit and at rest
- **Validation**: Validate all inputs and outputs
- **Logging**: Comprehensive security logging

### 4. Development Practices
- **Code Reviews**: Regular code quality assessments
- **Testing**: Unit, integration, and end-to-end tests
- **Documentation**: Maintain comprehensive documentation
- **Version Control**: Use branching strategies effectively

## Future Enhancements

### 1. Advanced Features
- **Machine Learning Integration**: AI-powered workflow optimization
- **Real-time Collaboration**: Multiple users working on same workflows
- **Cloud Deployment**: Serverless and containerized deployments
- **Microservices Architecture**: Service-oriented design

### 2. Technology Upgrades
- **WebAssembly**: Performance improvements for UI components
- **GraphQL**: Advanced API querying
- **Serverless Functions**: Event-driven computing
- **Container Orchestration**: Kubernetes integration

## Conclusion

Coconut Milk provides a comprehensive framework for building **task-based collaboration systems** with **visual programming capabilities**. Its **Lua runtime**, **WebView-based UI**, and **modular architecture** make it suitable for a wide range of automation and workflow management applications.

The system supports **multiple diagram types**, **extensive icon libraries**, and **cross-platform deployment**, making it an ideal choice for modern workflow automation solutions.

Key strengths include:
- **Flexible architecture** supporting various deployment scenarios
- **Rich visualization capabilities** for workflow design and analysis
- **Comprehensive documentation** and examples
- **Active development community** and regular updates
- **Production-ready** with enterprise-grade features

The project continues to evolve with new features and improvements, making it a valuable addition to the automation and workflow management ecosystem.
```